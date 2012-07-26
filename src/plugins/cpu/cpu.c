/**
 * CPU usage plugin to lxpanel
 *
 * Copyright (c) 2008 LxDE Developers, see the file AUTHORS for details.
 * Copyright (C) 2004 by Alexandre Pereira da Silva <alexandre.pereira@poli.usp.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
/*A little bug fixed by Mykola <mykola@2ka.mipt.ru>:) */

#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#define PLUGIN_PRIV_TYPE CPUPlugin

#include "gtkcompat.h"
#include "plugin.h"
#include "panel.h"
#include "misc.h"

#define BORDER_SIZE 2

#include "dbg.h"

typedef unsigned long CPUTick;			/* Value from /proc/stat */

typedef struct {
    float u, n, s;
} CPUSample;

struct cpu_stat {
    CPUTick u, n, s, i;				/* User, nice, system, idle */
};

/* Private context for CPU plugin. */
typedef struct {
    double foreground_color_u[3];
    double foreground_color_n[3];
    double foreground_color_s[3];
    double background_color[3];

    GtkWidget * da;				/* Drawing area */
    GdkPixmap * pixmap;				/* Pixmap to be drawn on drawing area */

    guint timer;				/* Timer for periodic update */
    CPUSample * stats_cpu;			/* Ring buffer of CPU utilization values */
    unsigned int ring_cursor;			/* Cursor for ring buffer */
    int pixmap_width;				/* Width of drawing area pixmap; also size of ring buffer; does not include border size */
    int pixmap_height;				/* Height of drawing area pixmap; does not include border size */
    struct cpu_stat previous_cpu_stat;		/* Previous value of cpu_stat */

    char * fg_color_u;
    char * fg_color_n;
    char * fg_color_s;
    char * bg_color;
    int update_interval;
} CPUPlugin;

static void redraw_pixmap(CPUPlugin * c);
static gboolean cpu_update(CPUPlugin * c);
static gboolean configure_event(GtkWidget * widget, GdkEventConfigure * event, CPUPlugin * c);
static gboolean expose_event(GtkWidget * widget, GdkEventExpose * event, CPUPlugin * c);
static int cpu_constructor(Plugin * p, char ** fp);
static void cpu_destructor(Plugin * p);
static void cpu_save_configuration(Plugin * p, FILE * fp);

/* Redraw after timer callback or resize. */
static void redraw_pixmap(CPUPlugin * c)
{
    cairo_t * cr = gdk_cairo_create(c->pixmap);

    /* Erase pixmap. */
    cairo_set_line_width (cr, 1.0);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

    cairo_set_source_rgb(cr, c->background_color[0], c->background_color[1], c->background_color[2]);
    cairo_rectangle(cr, 0, 0, c->pixmap_width, c->pixmap_height);
    cairo_fill(cr);

    /* Recompute pixmap. */

    unsigned int i;
    unsigned int drawing_cursor;

    cairo_set_source_rgb(cr, c->foreground_color_n[0], c->foreground_color_n[1], c->foreground_color_n[2]);
    for (i = 0, drawing_cursor = c->ring_cursor; i < c->pixmap_width; i++)
    {
        /* Draw one bar of the CPU usage graph. */
        float v = c->stats_cpu[drawing_cursor].u + c->stats_cpu[drawing_cursor].n + c->stats_cpu[drawing_cursor].s;
        if (v)
        {
            cairo_move_to (cr, i + 0.5, c->pixmap_height - 0.5);
            cairo_line_to (cr, i + 0.5, c->pixmap_height - v * c->pixmap_height - 0.5);
            cairo_stroke (cr);
        }

        /* Increment and wrap drawing cursor. */
        drawing_cursor += 1;
	if (drawing_cursor >= c->pixmap_width)
            drawing_cursor = 0;
    }

    cairo_set_source_rgb(cr, c->foreground_color_u[0], c->foreground_color_u[1], c->foreground_color_u[2]);
    for (i = 0, drawing_cursor = c->ring_cursor; i < c->pixmap_width; i++)
    {
        /* Draw one bar of the CPU usage graph. */
        float v = c->stats_cpu[drawing_cursor].u + c->stats_cpu[drawing_cursor].s;
        if (v)
        {
            cairo_move_to (cr, i + 0.5, c->pixmap_height - 0.5);
            cairo_line_to (cr, i + 0.5, c->pixmap_height - v * c->pixmap_height - 0.5);
            cairo_stroke (cr);
        }

        /* Increment and wrap drawing cursor. */
        drawing_cursor += 1;
	if (drawing_cursor >= c->pixmap_width)
            drawing_cursor = 0;
    }

    cairo_set_source_rgb(cr, c->foreground_color_s[0], c->foreground_color_s[1], c->foreground_color_s[2]);
    for (i = 0, drawing_cursor = c->ring_cursor; i < c->pixmap_width; i++)
    {
        /* Draw one bar of the CPU usage graph. */
        float v = c->stats_cpu[drawing_cursor].s;
        if (v)
        {
            cairo_move_to (cr, i + 0.5, c->pixmap_height - 0.5);
            cairo_line_to (cr, i + 0.5, c->pixmap_height - v * c->pixmap_height - 0.5);
            cairo_stroke (cr);
        }

        /* Increment and wrap drawing cursor. */
        drawing_cursor += 1;
	if (drawing_cursor >= c->pixmap_width)
            drawing_cursor = 0;
    }

    cairo_destroy(cr);

    /* Redraw pixmap. */
    gtk_widget_queue_draw(c->da);
}

/* Periodic timer callback. */
static gboolean cpu_update(CPUPlugin * c)
{
    if ((c->stats_cpu != NULL) && (c->pixmap != NULL))
    {
        /* Open statistics file and scan out CPU usage. */
        struct cpu_stat cpu;
        FILE * stat = fopen("/proc/stat", "r");
        if (stat == NULL)
            return TRUE;
        int fscanf_result = fscanf(stat, "cpu %lu %lu %lu %lu", &cpu.u, &cpu.n, &cpu.s, &cpu.i);
        fclose(stat);

        /* Ensure that fscanf succeeded. */
        if (fscanf_result == 4)
        {
            /* Compute delta from previous statistics. */
            struct cpu_stat cpu_delta;
            cpu_delta.u = cpu.u - c->previous_cpu_stat.u;
            cpu_delta.n = cpu.n - c->previous_cpu_stat.n;
            cpu_delta.s = cpu.s - c->previous_cpu_stat.s;
            cpu_delta.i = cpu.i - c->previous_cpu_stat.i;

            /* Copy current to previous. */
            memcpy(&c->previous_cpu_stat, &cpu, sizeof(struct cpu_stat));

            /* Compute user+nice+system as a fraction of total.
             * Introduce this sample to ring buffer, increment and wrap ring buffer cursor. */
            float cpu_uns = cpu_delta.u + cpu_delta.n + cpu_delta.s;
            float cpu_load = cpu_uns / (cpu_uns + cpu_delta.i);
            float cpu_load_u = cpu_delta.u / (cpu_uns + cpu_delta.i);
            float cpu_load_n = cpu_delta.n / (cpu_uns + cpu_delta.i);
            float cpu_load_s = cpu_delta.s / (cpu_uns + cpu_delta.i);

            c->stats_cpu[c->ring_cursor].u = cpu_load_u;
            c->stats_cpu[c->ring_cursor].n = cpu_load_n;
            c->stats_cpu[c->ring_cursor].s = cpu_load_s;

            c->ring_cursor += 1;
            if (c->ring_cursor >= c->pixmap_width)
                c->ring_cursor = 0;

            /* Redraw with the new sample. */
            redraw_pixmap(c);

            gchar * tooltip = g_strdup_printf(
                "Total: %.1f\nUser: %.1f\nNice: %.1f\nSystem: %.1f",
                cpu_load * 100, cpu_load_u * 100, cpu_load_n * 100, cpu_load_s * 100);
            gtk_widget_set_tooltip_text(c->da, tooltip);
            g_free(tooltip);
        }
    }
    return TRUE;
}

/* Handler for configure_event on drawing area. */
static gboolean configure_event(GtkWidget * widget, GdkEventConfigure * event, CPUPlugin * c)
{
    /* Allocate pixmap and statistics buffer without border pixels. */
    int new_pixmap_width = widget->allocation.width - BORDER_SIZE * 2;
    int new_pixmap_height = widget->allocation.height - BORDER_SIZE * 2;
    if ((new_pixmap_width > 0) && (new_pixmap_height > 0))
    {
        /* If statistics buffer does not exist or it changed size, reallocate and preserve existing data. */
        if ((c->stats_cpu == NULL) || (new_pixmap_width != c->pixmap_width))
        {
            CPUSample * new_stats_cpu = g_new0(typeof(*c->stats_cpu), new_pixmap_width);
            if (c->stats_cpu != NULL)
            {
                if (new_pixmap_width > c->pixmap_width)
                {
                    /* New allocation is larger.
                     * Introduce new "oldest" samples of zero following the cursor. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[0], c->ring_cursor * sizeof(CPUSample));
                    memcpy(&new_stats_cpu[new_pixmap_width - c->pixmap_width + c->ring_cursor],
                        &c->stats_cpu[c->ring_cursor], (c->pixmap_width - c->ring_cursor) * sizeof(CPUSample));
                }
                else if (c->ring_cursor <= new_pixmap_width)
                {
                    /* New allocation is smaller, but still larger than the ring buffer cursor.
                     * Discard the oldest samples following the cursor. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[0], c->ring_cursor * sizeof(CPUSample));
                    memcpy(&new_stats_cpu[c->ring_cursor],
                        &c->stats_cpu[c->pixmap_width - new_pixmap_width + c->ring_cursor], (new_pixmap_width - c->ring_cursor) * sizeof(CPUSample));
                }
                else
                {
                    /* New allocation is smaller, and also smaller than the ring buffer cursor.
                     * Discard all oldest samples following the ring buffer cursor and additional samples at the beginning of the buffer. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[c->ring_cursor - new_pixmap_width], new_pixmap_width * sizeof(CPUSample));
                    c->ring_cursor = 0;
                }
                g_free(c->stats_cpu);
            }
            c->stats_cpu = new_stats_cpu;
        }

        /* Allocate or reallocate pixmap. */
        c->pixmap_width = new_pixmap_width;
        c->pixmap_height = new_pixmap_height;
        if (c->pixmap)
            g_object_unref(c->pixmap);
        c->pixmap = gdk_pixmap_new(widget->window, c->pixmap_width, c->pixmap_height, -1);

        /* Redraw pixmap at the new size. */
        redraw_pixmap(c);
    }
    return TRUE;
}

/* Handler for expose_event on drawing area. */
static gboolean expose_event(GtkWidget * widget, GdkEventExpose * event, CPUPlugin * c)
{
    /* Draw the requested part of the pixmap onto the drawing area.
     * Translate it in both x and y by the border size. */
    if (c->pixmap != NULL)
    {
        cairo_t * cr = gdk_cairo_create(widget->window);

        gdk_cairo_set_source_pixmap(cr, c->pixmap, BORDER_SIZE, BORDER_SIZE);
        cairo_paint(cr);

        cairo_destroy(cr);
    }
    return FALSE;
}

/* Callback when the configuration dialog has recorded a configuration change. */
static void cpu_apply_configuration(Plugin * p)
{
    CPUPlugin * c = PRIV(p);

    /* Allocate top level widget and set into Plugin widget pointer. */
    if (!plugin_widget(p))
    {
        GtkWidget * pwid = gtk_event_box_new();
	plugin_set_widget(p, pwid);
        gtk_container_set_border_width(GTK_CONTAINER(pwid), 1);
        gtk_widget_set_has_window(pwid, FALSE);
    }

    /* Allocate drawing area as a child of top level widget.  Enable button press events. */
    if (!c->da)
    {
        c->da = gtk_drawing_area_new();
        gtk_widget_set_size_request(c->da, 40, PANEL_HEIGHT_DEFAULT);
        gtk_widget_add_events(c->da, GDK_BUTTON_PRESS_MASK);
        gtk_container_add(GTK_CONTAINER(plugin_widget(p)), c->da);

        /* Connect signals. */
        g_signal_connect(G_OBJECT(c->da), "configure_event", G_CALLBACK(configure_event), (gpointer) c);
        g_signal_connect(G_OBJECT(c->da), "expose_event", G_CALLBACK(expose_event), (gpointer) c);
        g_signal_connect(c->da, "button-press-event", G_CALLBACK(plugin_button_press_event), p);
    }


    color_parse_d(c->fg_color_u, c->foreground_color_u);
    color_parse_d(c->fg_color_n, c->foreground_color_n);
    color_parse_d(c->fg_color_s, c->foreground_color_s);
    color_parse_d(c->bg_color, c->background_color);

    /* Show the widget.  Connect a timer to refresh the statistics. */
    gtk_widget_show(c->da);
    if (c->timer)
        g_source_remove(c->timer);
    c->timer = g_timeout_add(c->update_interval, (GSourceFunc) cpu_update, (gpointer) c);
}

/* Plugin constructor. */
static int cpu_constructor(Plugin * p, char ** fp)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    CPUPlugin * c = g_new0(CPUPlugin, 1);
    plugin_set_priv(p, c);

    c->update_interval = 1500;

    /* Load parameters from the configuration file. */
    line s;
    if (fp)
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END)
        {
            if (s.type == LINE_NONE)
            {
                ERR( "cpu: illegal token %s\n", s.str);
                return 0;
            }
            if (s.type == LINE_VAR)
            {
                if (g_ascii_strcasecmp(s.t[0], "FgColorUser") == 0)
                    c->fg_color_u = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "FgColorNice") == 0)
                    c->fg_color_n = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "FgColorSystem") == 0)
                    c->fg_color_s = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "BgColor") == 0)
                    c->bg_color = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "UpdateInterval") == 0)
                    c->update_interval = atoi(s.t[1]);
                else
                    ERR( "cpu: unknown var %s\n", s.t[0]);
            }
            else
            {
                ERR( "cpu: illegal in this context %s\n", s.str);
                return 0;
            }
        }

    }


    #define DEFAULT_STRING(f, v) \
      if (c->f == NULL) \
          c->f = g_strdup(v);

    DEFAULT_STRING(fg_color_u, "green");
    DEFAULT_STRING(fg_color_n, "blue");
    DEFAULT_STRING(fg_color_s, "red");
    DEFAULT_STRING(bg_color, "black");

    #undef DEFAULT_STRING

    cpu_apply_configuration(p);

    return 1;
}

/* Plugin destructor. */
static void cpu_destructor(Plugin * p)
{
    CPUPlugin * c = PRIV(p);

    /* Disconnect the timer. */
    g_source_remove(c->timer);

    /* Deallocate memory. */
    g_object_unref(c->pixmap);
    g_free(c->stats_cpu);
    g_free(c->fg_color_u);
    g_free(c->fg_color_n);
    g_free(c->fg_color_s);
    g_free(c->bg_color);
    g_free(c);
}


/* Callback when the configuration dialog is to be shown. */
static void cpu_configure(Plugin * p, GtkWindow * parent)
{
    CPUPlugin * c = PRIV(p);

    int update_interval_min = 50;
    int update_interval_max = 5000;

    GtkWidget * dlg = create_generic_config_dlg(
        _(p->class->name),
        GTK_WIDGET(parent),
        (GSourceFunc) cpu_apply_configuration, (gpointer) p,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("Foreground color (user)"), &c->fg_color_u, (GType)CONF_TYPE_COLOR,
        _("Foreground color (nice)"), &c->fg_color_n, (GType)CONF_TYPE_COLOR,
        _("Foreground color (system)"), &c->fg_color_s, (GType)CONF_TYPE_COLOR,
        _("Background color"), &c->bg_color, (GType)CONF_TYPE_COLOR,
        _("Update interval" ), &c->update_interval, (GType)CONF_TYPE_INT,
        "int-min-value", (gpointer)&update_interval_min, (GType)CONF_TYPE_SET_PROPERTY,
        "int-max-value", (gpointer)&update_interval_max, (GType)CONF_TYPE_SET_PROPERTY,
        NULL);
    if (dlg)
        gtk_window_present(GTK_WINDOW(dlg));
}


/* Callback when the configuration is to be saved. */
static void cpu_save_configuration(Plugin * p, FILE * fp)
{
    CPUPlugin * c = PRIV(p);
    lxpanel_put_str(fp, "FgColorUser", c->fg_color_u);
    lxpanel_put_str(fp, "FgColorNice", c->fg_color_n);
    lxpanel_put_str(fp, "FgColorSystem", c->fg_color_s);
    lxpanel_put_str(fp, "BgColor", c->bg_color);
    lxpanel_put_int(fp, "UpdateInterval", c->update_interval);
}


/* Callback when panel configuration changes. */
static void cpu_panel_configuration_changed(Plugin * p)
{
    cpu_apply_configuration(p);
}


/* Plugin descriptor. */
PluginClass cpu_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "cpu",
    name : N_("CPU Usage Monitor"),
    version: "1.0",
    description : N_("Display CPU usage"),

    constructor : cpu_constructor,
    destructor  : cpu_destructor,
    config : cpu_configure,
    save : cpu_save_configuration,
    panel_configuration_changed : cpu_panel_configuration_changed
};
