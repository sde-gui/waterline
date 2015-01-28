/**
 * CPU usage plugin
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
#include <sde-utils-jansson.h>

#define PLUGIN_PRIV_TYPE CPUPlugin

#include <waterline/gtkcompat.h>
#include <waterline/plugin.h>
#include <waterline/panel.h>
#include <waterline/misc.h>

#define BORDER_SIZE 0

#include <waterline/dbg.h>

typedef unsigned long long CPUTick;			/* Value from /proc/stat */

enum {
    CPU_SAMPLE_S,
    CPU_SAMPLE_U,
    CPU_SAMPLE_N,
    CPU_SAMPLE_IO
};

typedef struct {
    float v[4];
} CPUSample;

struct cpu_stat {
    CPUTick u, n, s, i, io;				/* User, nice, system, idle, io wait */
};

/* Private context for CPU plugin. */
typedef struct {
    double foreground_color_io[3];
    double foreground_color_nice[3];
    double foreground_color_user[3];
    double foreground_color_system[3];
    double background_color[3];

    GtkWidget * frame;
    GtkWidget * da;				/* Drawing area */
    GdkPixmap * pixmap;				/* Pixmap to be drawn on drawing area */

    guint timer;				/* Timer for periodic update */
    CPUSample * stats_cpu;			/* Ring buffer of CPU utilization values */
    unsigned int ring_cursor;			/* Cursor for ring buffer */
    int pixmap_width;				/* Width of drawing area pixmap; also size of ring buffer; does not include border size */
    int pixmap_height;				/* Height of drawing area pixmap; does not include border size */
    struct cpu_stat previous_cpu_stat;		/* Previous value of cpu_stat */

    char * fg_color_io;
    char * fg_color_nice;
    char * fg_color_user;
    char * fg_color_system;
    char * bg_color;
    int update_interval;
} CPUPlugin;

/******************************************************************************/

#define SU_JSON_OPTION_STRUCTURE CPUPlugin
static su_json_option_definition option_definitions[] = {
    SU_JSON_OPTION(string, fg_color_io),
    SU_JSON_OPTION(string, fg_color_nice),
    SU_JSON_OPTION(string, fg_color_user),
    SU_JSON_OPTION(string, fg_color_system),
    SU_JSON_OPTION(string, bg_color),
    SU_JSON_OPTION(int, update_interval),
    {0,}
};

/******************************************************************************/



static void redraw_pixmap(CPUPlugin * c);
static gboolean cpu_update(CPUPlugin * c);
static gboolean configure_event(GtkWidget * widget, GdkEventConfigure * event, CPUPlugin * c);
static gboolean expose_event(GtkWidget * widget, GdkEventExpose * event, CPUPlugin * c);
static int cpu_constructor(Plugin * p);
static void cpu_destructor(Plugin * p);
static void cpu_save_configuration(Plugin * p);


static void draw_samples(CPUPlugin * c, cairo_t * cr, double color[3], int index)
{
    unsigned int i;
    unsigned int drawing_cursor;

    cairo_set_source_rgb(cr, color[0], color[1], color[2]);
    for (i = 0, drawing_cursor = c->ring_cursor; i < c->pixmap_width; i++)
    {
        /* Draw one bar of the CPU usage graph. */
        float v = 0;

        int j;
        for (j = 0; j <= index; j++)
            v += c->stats_cpu[drawing_cursor].v[j];

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
}

/* Redraw after timer callback or resize. */
static void redraw_pixmap(CPUPlugin * c)
{
    cairo_t * cr = gdk_cairo_create(c->pixmap);

    cairo_set_line_width (cr, 1.0);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

    /* Erase pixmap. */
    cairo_set_source_rgb(cr, c->background_color[0], c->background_color[1], c->background_color[2]);
    cairo_rectangle(cr, 0, 0, c->pixmap_width, c->pixmap_height);
    cairo_fill(cr);

    /* Recompute pixmap. */
    draw_samples(c, cr, c->foreground_color_io, CPU_SAMPLE_IO);
    draw_samples(c, cr, c->foreground_color_nice, CPU_SAMPLE_N);
    draw_samples(c, cr, c->foreground_color_user, CPU_SAMPLE_U);
    draw_samples(c, cr, c->foreground_color_system, CPU_SAMPLE_S);

    cairo_destroy(cr);

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
        int fscanf_result = fscanf(stat, "cpu %llu %llu %llu %llu %llu", &cpu.u, &cpu.n, &cpu.s, &cpu.i, &cpu.io);
        fclose(stat);

        /* Ensure that fscanf succeeded. */
        if (fscanf_result == 5)
        {
            /* Compute delta from previous statistics. */
            struct cpu_stat cpu_delta;
            cpu_delta.u = cpu.u - c->previous_cpu_stat.u;
            cpu_delta.n = cpu.n - c->previous_cpu_stat.n;
            cpu_delta.s = cpu.s - c->previous_cpu_stat.s;
            cpu_delta.i = cpu.i - c->previous_cpu_stat.i;
            cpu_delta.io = cpu.io - c->previous_cpu_stat.io;


            /* Copy current to previous. */
            memcpy(&c->previous_cpu_stat, &cpu, sizeof(struct cpu_stat));

            float cpu_notidle = cpu_delta.u + cpu_delta.n + cpu_delta.s + cpu_delta.io;
            float cpu_total = cpu_notidle + cpu_delta.i;
            float cpu_load = cpu_notidle / cpu_total;
            float cpu_load_u = cpu_delta.u / cpu_total;
            float cpu_load_n = cpu_delta.n / cpu_total;
            float cpu_load_s = cpu_delta.s / cpu_total;
            float cpu_load_io = cpu_delta.io / cpu_total;

            c->stats_cpu[c->ring_cursor].v[CPU_SAMPLE_U] = cpu_load_u;
            c->stats_cpu[c->ring_cursor].v[CPU_SAMPLE_N] = cpu_load_n;
            c->stats_cpu[c->ring_cursor].v[CPU_SAMPLE_S] = cpu_load_s;
            c->stats_cpu[c->ring_cursor].v[CPU_SAMPLE_IO] = cpu_load_io;

            c->ring_cursor += 1;
            if (c->ring_cursor >= c->pixmap_width)
                c->ring_cursor = 0;

            /* Redraw with the new sample. */
            redraw_pixmap(c);

            gchar * tooltip = g_strdup_printf(
                "Total: %.1f\nIOWait %.1f\nNice: %.1f\nUser: %.1f\nSystem: %.1f",
                cpu_load * 100, cpu_load_io * 100, cpu_load_n * 100, cpu_load_u * 100, cpu_load_s * 100);
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

        c->frame = gtk_frame_new(NULL);
        gtk_container_add(GTK_CONTAINER(plugin_widget(p)), c->frame);
        gtk_frame_set_shadow_type(GTK_FRAME(c->frame), GTK_SHADOW_IN);
    }

    /* Allocate drawing area as a child of top level widget.  Enable button press events. */
    if (!c->da)
    {
        c->da = gtk_drawing_area_new();
        gtk_widget_set_size_request(c->da, 40, plugin_get_icon_size(p));
        gtk_widget_add_events(c->da, GDK_BUTTON_PRESS_MASK);
        gtk_container_add(GTK_CONTAINER(c->frame), c->da);

        /* Connect signals. */
        g_signal_connect(G_OBJECT(c->da), "configure_event", G_CALLBACK(configure_event), (gpointer) c);
        g_signal_connect(G_OBJECT(c->da), "expose_event", G_CALLBACK(expose_event), (gpointer) c);
        g_signal_connect(c->da, "button-press-event", G_CALLBACK(plugin_button_press_event), p);
    }

    color_parse_d(c->fg_color_io, c->foreground_color_io);
    color_parse_d(c->fg_color_nice, c->foreground_color_nice);
    color_parse_d(c->fg_color_user, c->foreground_color_user);
    color_parse_d(c->fg_color_system, c->foreground_color_system);
    color_parse_d(c->bg_color, c->background_color);

    gtk_widget_show_all(c->frame);

    if (c->timer)
        g_source_remove(c->timer);
    c->timer = g_timeout_add(c->update_interval, (GSourceFunc) cpu_update, (gpointer) c);
}

/* Plugin constructor. */
static int cpu_constructor(Plugin * p)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    CPUPlugin * c = g_new0(CPUPlugin, 1);
    plugin_set_priv(p, c);

    c->update_interval = 1500;
    c->fg_color_io = g_strdup("grey");
    c->fg_color_nice = g_strdup("blue");
    c->fg_color_user = g_strdup("green");
    c->fg_color_system = g_strdup("red");
    c->bg_color = g_strdup("black");

    su_json_read_options(plugin_inner_json(p), option_definitions, c);

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
    g_free(c->fg_color_user);
    g_free(c->fg_color_nice);
    g_free(c->fg_color_system);
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
        _(plugin_class(p)->name),
        GTK_WIDGET(parent),
        (GSourceFunc) cpu_apply_configuration, (gpointer) p,

        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,

        _("Colors"), 0, (GType)CONF_TYPE_TITLE,
        _("IOWait"), &c->fg_color_io, (GType)CONF_TYPE_COLOR,
        _("Nice"), &c->fg_color_nice, (GType)CONF_TYPE_COLOR,
        _("User"), &c->fg_color_user, (GType)CONF_TYPE_COLOR,
        _("System"), &c->fg_color_system, (GType)CONF_TYPE_COLOR,
        _("Idle"), &c->bg_color, (GType)CONF_TYPE_COLOR,

        _("Options"), 0, (GType)CONF_TYPE_TITLE,
        _("Update interval" ), &c->update_interval, (GType)CONF_TYPE_INT,
        "int-min-value", (gpointer)&update_interval_min, (GType)CONF_TYPE_SET_PROPERTY,
        "int-max-value", (gpointer)&update_interval_max, (GType)CONF_TYPE_SET_PROPERTY,
        NULL);

    if (dlg)
        gtk_window_present(GTK_WINDOW(dlg));
}


/* Callback when the configuration is to be saved. */
static void cpu_save_configuration(Plugin * p)
{
    CPUPlugin * c = PRIV(p);
    su_json_write_options(plugin_inner_json(p), option_definitions, c);
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
    category: PLUGIN_CATEGORY_HW_INDICATOR,

    constructor : cpu_constructor,
    destructor  : cpu_destructor,
    config : cpu_configure,
    save : cpu_save_configuration,
    panel_configuration_changed : cpu_panel_configuration_changed
};
