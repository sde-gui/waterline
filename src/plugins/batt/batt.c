/*
 * ACPI battery monitor plugin for LXPanel
 *
 * Copyright (C) 2007 by Greg McNew <gmcnew@gmail.com>
 * Copyright (C) 2008 by Hong Jen Yee <pcman.tw@gmail.com>
 * Copyright (C) 2009 by Juergen Hoetzel <juergen@archlinux.org>
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
 *
 * This plugin monitors battery usage on ACPI-enabled systems by reading the
 * battery information found in /sys/class/power_supply. The update interval is
 * user-configurable and defaults to 3 second.
 *
 * The battery's remaining life is estimated from its current charge and current
 * rate of discharge. The user may configure an alarm command to be run when
 * their estimated remaining battery life reaches a certain level.
 */

/* FIXME:
 *  Here are somethings need to be improvec:
 *  1. Replace pthread stuff with gthread counterparts for portability.
 *  3. Add an option to hide the plugin when AC power is used or there is no battery.
 *  4. Handle failure gracefully under systems other than Linux.
*/

#include <glib.h>
#include <glib/gi18n.h>
#include <pthread.h> /* used by pthread_create() and alarmThread */
#include <semaphore.h> /* used by update() and alarmProcess() for alarms */
#include <stdlib.h>
#include <string.h>

#include "gtkcompat.h"
#include "dbg.h"
#include "batt_sys.h"
#include "misc.h" /* used for the line struct */
#include "panel.h" /* used to determine panel orientation */
#include "plugin.h"

/* The last MAX_SAMPLES samples are averaged when charge rates are evaluated.
   This helps prevent spikes in the "time left" values the user sees. */
#define MAX_SAMPLES 10

enum {
    DISPLAY_AS_BAR,
    DISPLAY_AS_TEXT
};

static pair display_as_pair[] = {
    { DISPLAY_AS_BAR, "Bar"},
    { DISPLAY_AS_TEXT, "Text"},
    { 0, NULL},
};

typedef struct {
    char *alarmCommand,
        *backgroundColor,
        *chargingColor1,
        *chargingColor2,
        *dischargingColor1,
        *dischargingColor2;

    double background_color[3];
    double charging1_color[3];
    double charging2_color[3];
    double discharging1_color[3];
    double discharging2_color[3];

    GtkWidget *vbox;
    GtkWidget *hbox;
    GdkPixmap *pixmap;
    GtkWidget *drawingArea;
    GtkWidget *label;

    int display_as;

    int orientation;
    unsigned int alarmTime,
        border,
        height,
        length,
        numSamples,
        *rateSamples,
        rateSamplesSum,
        timer,
        state_elapsed_time,
        info_elapsed_time,
        wasCharging,
        width,
        hide_if_no_battery;
    sem_t alarmProcessLock;
    battery* b;
    gboolean has_ac_adapter;

    int bar_preferred_width;
    int bar_preferred_height;
} lx_battery;


typedef struct {
    char *command;
    sem_t *lock;
} Alarm;

static void destructor(Plugin *p);
static void update_display(lx_battery *lx_b);
static void batt_panel_configuration_changed(Plugin *p);

/* alarmProcess takes the address of a dynamically allocated alarm struct (which
   it must free). It ensures that alarm commands do not run concurrently. */
static void * alarmProcess(void *arg) {
    Alarm *a = (Alarm *) arg;

    sem_wait(a->lock);
    system(a->command);
    sem_post(a->lock);

    g_free(a);
    return NULL;
}

static void get_status_color(lx_battery *lx_b, double color[3])
{
    if (!lx_b->b)
    {
       color[0] = 0;
       color[1] = 0;
       color[2] = 0;
    }

    gboolean isCharging = battery_is_charging(lx_b->b);

    double v = lx_b->b->percentage / 100.0;

    if (isCharging)
    {
        color[0] = lx_b->charging1_color[0] * v + lx_b->charging2_color[0] * (1.0 - v);
        color[1] = lx_b->charging1_color[1] * v + lx_b->charging2_color[1] * (1.0 - v);
        color[2] = lx_b->charging1_color[2] * v + lx_b->charging2_color[2] * (1.0 - v);
    }
    else
    {
        color[0] = lx_b->discharging1_color[0] * v + lx_b->discharging2_color[0] * (1.0 - v);
        color[1] = lx_b->discharging1_color[1] * v + lx_b->discharging2_color[1] * (1.0 - v);
        color[2] = lx_b->discharging1_color[2] * v + lx_b->discharging2_color[2] * (1.0 - v);
    }

}

static void update_bar(lx_battery *lx_b)
{
    if (!lx_b->pixmap)
        return;

    if (!lx_b->b)
        return;


    /* Bar color */

    double bar_color[3];
    get_status_color(lx_b, bar_color);

    double v = 0.3;

    double background_color1[3];

    background_color1[0] = bar_color[0] * v + lx_b->background_color[0] * (1.0 - v);
    background_color1[1] = bar_color[1] * v + lx_b->background_color[1] * (1.0 - v);
    background_color1[2] = bar_color[2] * v + lx_b->background_color[2] * (1.0 - v);

    int border = lx_b->border;
    while (1)
    {
        int barroom = MIN(lx_b->width, lx_b->height) - border * 2;
        if (barroom >= border || border < 1)
           break;
        border--;
    }


    cairo_t * cr = gdk_cairo_create(lx_b->pixmap);

    /* Erase pixmap. */

    cairo_set_line_width (cr, 1.0);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

    cairo_set_source_rgb(cr, background_color1[0], background_color1[1], background_color1[2]);
    cairo_rectangle(cr, 0, 0, lx_b->width, lx_b->height);
    cairo_fill(cr);

    /* Draw border. */

    cairo_set_line_width (cr, border);
    cairo_set_source_rgb(cr, lx_b->background_color[0], lx_b->background_color[1], lx_b->background_color[2]);
    cairo_rectangle(cr, border / 2.0, border / 2.0, lx_b->width - border, lx_b->height - border);
    cairo_stroke(cr);


    /* Draw bar. */

    cairo_set_source_rgb(cr, bar_color[0], bar_color[1], bar_color[2]);

    int chargeLevel = lx_b->b->percentage * (lx_b->length - 2 * border) / 100;

    if (lx_b->orientation == ORIENT_HORIZ)
    {
        cairo_rectangle(cr,
            border,
            lx_b->height - border - chargeLevel,
            lx_b->width - border * 2,
            chargeLevel);
    }
    else
    {
        cairo_rectangle(cr,
            border,
            border,
            chargeLevel,
            lx_b->height - border * 2);
    }
    cairo_fill(cr);

    cairo_destroy(cr);

    gtk_widget_queue_draw(lx_b->drawingArea);
}

void update_label(lx_battery *lx_b) {

    if (!lx_b->label)
        return;

    if (!lx_b->b)
        return;


    gboolean isCharging = battery_is_charging(lx_b->b);

    gchar * text = NULL;
    if (isCharging)
        text = g_strdup_printf(_("%d%%↑"), lx_b->b->percentage);
    else
        text = g_strdup_printf(_("%d%%↓"), lx_b->b->percentage);

    double status_color[3];
    get_status_color(lx_b, status_color);
    int color = (((int) (status_color[0] * 255)) << 16) +
                (((int) (status_color[1] * 255)) << 8) +
                 ((int) (status_color[2] * 255));
    
    gchar * markup = g_strdup_printf("<span color=\"#%06x\">%s</span>", color, text);

    gtk_label_set_markup(GTK_LABEL(lx_b->label), markup);

    g_free(markup);
    g_free(text);
}

/* FIXME:
   Don't repaint if percentage of remaining charge and remaining time aren't changed. */
void update_display(lx_battery *lx_b) {
    char tooltip[ 256 ];
    battery *b = lx_b->b;
    /* unit: mW */


    /* no battery is found */
    if( b == NULL ) 
    {
	gtk_widget_set_tooltip_text( lx_b->vbox, _("No batteries found") );

        if (!lx_b->label)
        {
            lx_b->label = gtk_label_new("");
            gtk_box_pack_start(GTK_BOX(lx_b->hbox), lx_b->label, TRUE, FALSE, 0);
        }

        gtk_widget_show(lx_b->label);
        gtk_widget_hide(lx_b->drawingArea);

        gtk_label_set_text(GTK_LABEL(lx_b->label), _("N/A"));

	return;
    }
    
    /* fixme: only one battery supported */

    int rate = lx_b->b->current_now;
    gboolean isCharging = battery_is_charging ( b );
    
    /* Consider running the alarm command */
    if ( !isCharging && rate > 0 &&
	( ( battery_get_remaining( b ) / 60 ) < lx_b->alarmTime ) )
    {
	/* Shrug this should be done using glibs process functions */
	/* Alarms should not run concurrently; determine whether an alarm is
	   already running */
	int alarmCanRun;
	sem_getvalue(&(lx_b->alarmProcessLock), &alarmCanRun);
	
	/* Run the alarm command if it isn't already running */
	if (alarmCanRun) {
	    
	    Alarm *a = (Alarm *) malloc(sizeof(Alarm));
	    a->command = lx_b->alarmCommand;
	    a->lock = &(lx_b->alarmProcessLock);
	    
	    /* Manage the alarm process in a new thread, which which will be
	       responsible for freeing the alarm struct it's given */
	    pthread_t alarmThread;
	    pthread_create(&alarmThread, NULL, alarmProcess, a);
	    
	}
    }

    /* Make a tooltip string, and display remaining charge time if the battery
       is charging or remaining life if it's discharging */
    if (isCharging) {
	int hours = lx_b->b->seconds / 3600;
	int left_seconds = b->seconds - 3600 * hours;
	int minutes = left_seconds / 60;
	snprintf(tooltip, 256,
		_("Battery: %d%% charged, %d:%02d until full"),
		lx_b->b->percentage,
		hours,
		minutes );
    } else {
	/* if we have enough rate information for battery */
	if (lx_b->b->percentage != 100) {
	    int hours = lx_b->b->seconds / 3600;
	    int left_seconds = b->seconds - 3600 * hours;
	    int minutes = left_seconds / 60;
	    snprintf(tooltip, 256,
		    _("Battery: %d%% charged, %d:%02d left"),
		    lx_b->b->percentage,
		    hours,
		    minutes );
	} else {
	    snprintf(tooltip, 256,
		    _("Battery: %d%% charged"),
		    100 );
	}
    }

    gtk_widget_set_tooltip_text(lx_b->vbox, tooltip);

    if (lx_b->display_as == DISPLAY_AS_BAR)
    {
        if (lx_b->label)
            gtk_widget_hide(lx_b->label);
        gtk_widget_show(lx_b->drawingArea);
        update_bar(lx_b);
    }
    else
    {
        if (!lx_b->label)
        {
            lx_b->label = gtk_label_new("");
            gtk_box_pack_start(GTK_BOX(lx_b->hbox), lx_b->label, TRUE, FALSE, 0);
        }
        gtk_widget_show(lx_b->label);
        gtk_widget_hide(lx_b->drawingArea);
        update_label(lx_b);
    }

}

/* This callback is called every 3 seconds */
static int update_timout(lx_battery *lx_b) {
    GDK_THREADS_ENTER();
    lx_b->state_elapsed_time++;
    lx_b->info_elapsed_time++;

    /* check the batteries every 3 seconds */
    if (lx_b->b)
        battery_update( lx_b->b );
    else
        lx_b->b = battery_get();

    update_display(lx_b);

    GDK_THREADS_LEAVE();
    return TRUE;
}

/* An update will be performed whenever the user clicks on the charge bar */
static gint buttonPressEvent(GtkWidget *widget, GdkEventButton *event,
        Plugin* plugin) {

    lx_battery *lx_b = (lx_battery*)plugin->priv;

    update_display(lx_b);

    if( event->button == 3 )  /* right button */
    {
        lxpanel_show_panel_menu( plugin->panel, plugin, event );
        return TRUE;
    }
    return FALSE;
}


static gint configureEvent(GtkWidget *widget, GdkEventConfigure *event,
        lx_battery *lx_b) {

    ENTER;

    if (lx_b->width == widget->allocation.width && lx_b->height == widget->allocation.height)
    {
        RET(TRUE);
    }

    //g_print("allocation: %d, %d\n", widget->allocation.width, widget->allocation.height);

    /* Update the plugin's dimensions */
    lx_b->width = widget->allocation.width;
    lx_b->height = widget->allocation.height;
    if (lx_b->orientation == ORIENT_HORIZ) {
        lx_b->length = lx_b->height;
    }
    else {
        lx_b->length = lx_b->width;
    }

    if (lx_b->pixmap)
        g_object_unref(lx_b->pixmap);

    lx_b->pixmap = gdk_pixmap_new (widget->window, widget->allocation.width,
          widget->allocation.height, -1);

    /* Perform an update so the bar will look right in its new orientation */
    update_display(lx_b);

    RET(TRUE);

}

static void sizeRequest(GtkWidget * widget, GtkRequisition * requisition, lx_battery *lx_b)
{
    requisition->width = lx_b->bar_preferred_width;
    requisition->height = lx_b->bar_preferred_height;
    //g_print("requisition: %d, %d\n", lx_b->bar_preferred_width, lx_b->bar_preferred_height);
}

static gint exposeEvent(GtkWidget *widget, GdkEventExpose *event, lx_battery *lx_b) {

    ENTER;

    gdk_draw_drawable (widget->window, lx_b->drawingArea->style->black_gc,
            lx_b->pixmap, event->area.x, event->area.y, event->area.x,
            event->area.y, event->area.width, event->area.height);

    RET(FALSE);

}


static int
constructor(Plugin *p, char **fp)
{
    ENTER;

    lx_battery *lx_b;
    p->priv = lx_b = g_new0(lx_battery, 1);

    /* get available battery */
    lx_b->b = battery_get ();
    
    /* no battery available */
/*    if ( lx_b->b == NULL )
	goto error;*/
    
    p->pwid = gtk_event_box_new();
    gtk_widget_set_has_window(p->pwid, FALSE);
    gtk_container_set_border_width( GTK_CONTAINER(p->pwid), 1 );

    lx_b->vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(p->pwid), lx_b->vbox);
    gtk_widget_show(lx_b->vbox);

    lx_b->hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(lx_b->vbox), lx_b->hbox, TRUE, FALSE, 0);
    gtk_widget_show(lx_b->hbox);

    lx_b->drawingArea = gtk_drawing_area_new();
    gtk_widget_add_events( lx_b->drawingArea, GDK_BUTTON_PRESS_MASK );
    gtk_box_pack_start(GTK_BOX(lx_b->hbox), lx_b->drawingArea, TRUE, FALSE, 0);

    gtk_widget_show(lx_b->drawingArea);

    g_signal_connect (G_OBJECT (p->pwid), "button_press_event",
            G_CALLBACK(buttonPressEvent), (gpointer) p);
    g_signal_connect (G_OBJECT (lx_b->drawingArea),"configure_event",
          G_CALLBACK (configureEvent), (gpointer) lx_b);
    g_signal_connect (G_OBJECT (lx_b->drawingArea), "size-request",
          G_CALLBACK (sizeRequest), (gpointer) lx_b);
    g_signal_connect (G_OBJECT (lx_b->drawingArea), "expose_event",
          G_CALLBACK (exposeEvent), (gpointer) lx_b);

    sem_init(&(lx_b->alarmProcessLock), 0, 1);

    lx_b->alarmCommand = lx_b->backgroundColor = lx_b->chargingColor1 = lx_b->chargingColor2
            = lx_b->dischargingColor1 = lx_b->dischargingColor2 = NULL;

    /* Set default values for integers */
    lx_b->alarmTime = 5;
    lx_b->border = 3;
    lx_b->display_as = DISPLAY_AS_TEXT;

    line s;

    if (fp) {

        /* Apply options */
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END) {
            if (s.type == LINE_NONE) {
                ERR( "batt: illegal token %s\n", s.str);
                goto error;
            }
            if (s.type == LINE_VAR) {
                if (!g_ascii_strcasecmp(s.t[0], "HideIfNoBattery"))
                    lx_b->hide_if_no_battery = atoi(s.t[1]);
                else if (!g_ascii_strcasecmp(s.t[0], "AlarmCommand"))
                    lx_b->alarmCommand = g_strdup(s.t[1]);
                else if (!g_ascii_strcasecmp(s.t[0], "BackgroundColor"))
                    lx_b->backgroundColor = g_strdup(s.t[1]);
                else if (!g_ascii_strcasecmp(s.t[0], "ChargingColor1"))
                    lx_b->chargingColor1 = g_strdup(s.t[1]);
                else if (!g_ascii_strcasecmp(s.t[0], "ChargingColor2"))
                    lx_b->chargingColor2 = g_strdup(s.t[1]);
                else if (!g_ascii_strcasecmp(s.t[0], "DischargingColor1"))
                    lx_b->dischargingColor1 = g_strdup(s.t[1]);
                else if (!g_ascii_strcasecmp(s.t[0], "DischargingColor2"))
                    lx_b->dischargingColor2 = g_strdup(s.t[1]);
                else if (!g_ascii_strcasecmp(s.t[0], "AlarmTime"))
                    lx_b->alarmTime = atoi(s.t[1]);
                else if (!g_ascii_strcasecmp(s.t[0], "BorderWidth"))
                    lx_b->border = MAX(0, atoi(s.t[1]));
                else if (g_ascii_strcasecmp(s.t[0], "DisplayAs") == 0)
                    lx_b->display_as = str2num(display_as_pair, s.t[1], lx_b->display_as);
                else {
                    ERR( "batt: unknown var %s\n", s.t[0]);
                    continue;
                }
            }
            else {
                ERR( "batt: illegal in this context %s\n", s.str);
                goto error;
            }
        }

    }

    /* Apply more default options */
    if (! lx_b->alarmCommand)
        lx_b->alarmCommand = g_strdup("xmessage Battery low");
    if (! lx_b->backgroundColor)
        lx_b->backgroundColor = g_strdup("black");
    if (! lx_b->chargingColor1)
        lx_b->chargingColor1 = g_strdup("#1B3BC6");
    if (! lx_b->chargingColor2)
        lx_b->chargingColor2 = g_strdup("#B52FC3");
    if (! lx_b->dischargingColor1)
        lx_b->dischargingColor1 = g_strdup("#00FF00");
    if (! lx_b->dischargingColor2)
        lx_b->dischargingColor2 = g_strdup("#FF0000");

    color_parse_d(lx_b->backgroundColor, lx_b->background_color);
    color_parse_d(lx_b->chargingColor1, lx_b->charging1_color);
    color_parse_d(lx_b->chargingColor2, lx_b->charging2_color);
    color_parse_d(lx_b->dischargingColor1, lx_b->discharging1_color);
    color_parse_d(lx_b->dischargingColor2, lx_b->discharging2_color);

    batt_panel_configuration_changed(p);

    /* Start the update loop */
    lx_b->timer = g_timeout_add_seconds( 3, (GSourceFunc) update_timout, (gpointer) lx_b);

    RET(TRUE);

error:
    RET(FALSE);
}


static void
destructor(Plugin *p)
{
    ENTER;

    lx_battery *b = (lx_battery *) p->priv;

    if (b->pixmap)
        g_object_unref(b->pixmap);

    g_free(b->alarmCommand);
    g_free(b->backgroundColor);
    g_free(b->chargingColor1);
    g_free(b->chargingColor2);
    g_free(b->dischargingColor1);
    g_free(b->dischargingColor2);

    g_free(b->rateSamples);
    sem_destroy(&(b->alarmProcessLock));
    if (b->timer)
        g_source_remove(b->timer);
    g_free(b);

    RET();

}


static void batt_panel_configuration_changed(Plugin *p) {

    ENTER;

    lx_battery *b = (lx_battery *) p->priv;

    b->orientation = panel_get_orientation(p->panel);
    
    if (b->orientation == ORIENT_HORIZ)
    {
        b->bar_preferred_height = panel_get_icon_size(p->panel);;
        b->bar_preferred_width = b->bar_preferred_height / 3;
        if (b->bar_preferred_width < 5)
            b->bar_preferred_width = 5;
    }
    else
    {
        b->bar_preferred_width = panel_get_icon_size(p->panel);;
        b->bar_preferred_height = b->bar_preferred_width / 3;
        if (b->bar_preferred_height < 5)
            b->bar_preferred_height = 5;
    }
    gtk_widget_queue_resize(b->drawingArea);
    
    RET();
}


static void applyConfig(Plugin* p)
{
    ENTER;

    lx_battery *b = (lx_battery *) p->priv;

    /* Update colors */
    color_parse_d(b->backgroundColor, b->background_color);
    color_parse_d(b->chargingColor1, b->charging1_color);
    color_parse_d(b->chargingColor2, b->charging2_color);
    color_parse_d(b->dischargingColor1, b->discharging1_color);
    color_parse_d(b->dischargingColor2, b->discharging2_color);

    /* Make sure the border value is acceptable */
    b->border = MAX(0, b->border);

    update_display(b);

    RET();
}


static void config(Plugin *p, GtkWindow* parent) {
    ENTER;

    GtkWidget *dialog;
    lx_battery *b = (lx_battery *) p->priv;
    dialog = create_generic_config_dlg(_(p->class->name),
            GTK_WIDGET(parent),
            (GSourceFunc) applyConfig, (gpointer) p,
#if 0
            _("Hide if there is no battery"), &b->hide_if_no_battery, (GType)CONF_TYPE_BOOL,
#endif
            "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
            _("Alarm command"), &b->alarmCommand, (GType)CONF_TYPE_STR,
            _("Alarm time (minutes left)"), &b->alarmTime, (GType)CONF_TYPE_INT,

            _("|Display as:|Bar|Text"), &b->display_as, (GType)CONF_TYPE_ENUM,
            _("Background color"), &b->backgroundColor, (GType)CONF_TYPE_COLOR,
            _("Charging color 1"), &b->chargingColor1, (GType)CONF_TYPE_COLOR,
            _("Charging color 2"), &b->chargingColor2, (GType)CONF_TYPE_COLOR,
            _("Discharging color 1"), &b->dischargingColor1, (GType)CONF_TYPE_COLOR,
            _("Discharging color 2"), &b->dischargingColor2, (GType)CONF_TYPE_COLOR,
            _("Border width"), &b->border, (GType)CONF_TYPE_INT,
            NULL);
    if (dialog)
        gtk_window_present(GTK_WINDOW(dialog));

    RET();
}


static void save(Plugin* p, FILE* fp) {
    lx_battery *lx_b = (lx_battery *) p->priv;

    lxpanel_put_bool(fp, "HideIfNoBattery",lx_b->hide_if_no_battery);
    lxpanel_put_str(fp, "AlarmCommand", lx_b->alarmCommand);
    lxpanel_put_int(fp, "AlarmTime", lx_b->alarmTime);
    lxpanel_put_enum(fp, "DisplayAs", lx_b->display_as, display_as_pair);
    lxpanel_put_str(fp, "BackgroundColor", lx_b->backgroundColor);
    lxpanel_put_int(fp, "BorderWidth", lx_b->border);
    lxpanel_put_str(fp, "ChargingColor1", lx_b->chargingColor1);
    lxpanel_put_str(fp, "ChargingColor2", lx_b->chargingColor2);
    lxpanel_put_str(fp, "DischargingColor1", lx_b->dischargingColor1);
    lxpanel_put_str(fp, "DischargingColor2", lx_b->dischargingColor2);
}


PluginClass batt_plugin_class = {
    
    PLUGINCLASS_VERSIONING,

    type        : "batt",
    name        : N_("Battery Monitor"),
    version     : "2.0",
    description : N_("Display battery status using ACPI"),

    constructor : constructor,
    destructor  : destructor,
    config      : config,
    save        : save,
    panel_configuration_changed : batt_panel_configuration_changed
};
