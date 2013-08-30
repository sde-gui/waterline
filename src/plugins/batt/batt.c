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

#define PLUGIN_PRIV_TYPE BatteryPlugin

#include <lxpanelx/gtkcompat.h>
#include <lxpanelx/dbg.h>
#include "batt_sys.h"
#include <lxpanelx/misc.h>
#include <lxpanelx/panel.h>
#include <lxpanelx/plugin.h>

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
        border_width,
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
} BatteryPlugin;


typedef struct {
    char *command;
    sem_t *lock;
} Alarm;

static void destructor(Plugin *p);
static void update_display(BatteryPlugin *iplugin);
static void batt_panel_configuration_changed(Plugin *p);

/******************************************************************************/

#define WTL_JSON_OPTION_STRUCTURE BatteryPlugin
static wtl_json_option_definition option_definitions[] = {
    WTL_JSON_OPTION(bool, hide_if_no_battery),
    WTL_JSON_OPTION(string, alarmCommand),
    WTL_JSON_OPTION(int, alarmTime),
    WTL_JSON_OPTION_ENUM(display_as_pair, display_as),
    WTL_JSON_OPTION(string, backgroundColor),
    WTL_JSON_OPTION(int, border_width),
    WTL_JSON_OPTION(string, chargingColor1),
    WTL_JSON_OPTION(string, chargingColor2),
    WTL_JSON_OPTION(string, dischargingColor1),
    WTL_JSON_OPTION(string, dischargingColor2),
    {0,}
};

/******************************************************************************/



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

static void get_status_color(BatteryPlugin *iplugin, double color[3])
{
    if (!iplugin->b)
    {
       color[0] = 0;
       color[1] = 0;
       color[2] = 0;
    }

    gboolean isCharging = battery_is_charging(iplugin->b);

    double v = iplugin->b->percentage / 100.0;

    if (isCharging)
    {
        color[0] = iplugin->charging1_color[0] * v + iplugin->charging2_color[0] * (1.0 - v);
        color[1] = iplugin->charging1_color[1] * v + iplugin->charging2_color[1] * (1.0 - v);
        color[2] = iplugin->charging1_color[2] * v + iplugin->charging2_color[2] * (1.0 - v);
    }
    else
    {
        color[0] = iplugin->discharging1_color[0] * v + iplugin->discharging2_color[0] * (1.0 - v);
        color[1] = iplugin->discharging1_color[1] * v + iplugin->discharging2_color[1] * (1.0 - v);
        color[2] = iplugin->discharging1_color[2] * v + iplugin->discharging2_color[2] * (1.0 - v);
    }

}

static void update_bar(BatteryPlugin *iplugin)
{
    if (!iplugin->pixmap)
        return;

    if (!iplugin->b)
        return;


    /* Bar color */

    double bar_color[3];
    get_status_color(iplugin, bar_color);

    double v = 0.3;

    double background_color1[3];

    background_color1[0] = bar_color[0] * v + iplugin->background_color[0] * (1.0 - v);
    background_color1[1] = bar_color[1] * v + iplugin->background_color[1] * (1.0 - v);
    background_color1[2] = bar_color[2] * v + iplugin->background_color[2] * (1.0 - v);

    int border = iplugin->border_width;
    while (1)
    {
        int barroom = MIN(iplugin->width, iplugin->height) - border * 2;
        if (barroom >= border || border < 1)
           break;
        border--;
    }


    cairo_t * cr = gdk_cairo_create(iplugin->pixmap);

    /* Erase pixmap. */

    cairo_set_line_width (cr, 1.0);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

    cairo_set_source_rgb(cr, background_color1[0], background_color1[1], background_color1[2]);
    cairo_rectangle(cr, 0, 0, iplugin->width, iplugin->height);
    cairo_fill(cr);

    /* Draw border. */

    cairo_set_line_width (cr, border);
    cairo_set_source_rgb(cr, iplugin->background_color[0], iplugin->background_color[1], iplugin->background_color[2]);
    cairo_rectangle(cr, border / 2.0, border / 2.0, iplugin->width - border, iplugin->height - border);
    cairo_stroke(cr);


    /* Draw bar. */

    cairo_set_source_rgb(cr, bar_color[0], bar_color[1], bar_color[2]);

    int chargeLevel = iplugin->b->percentage * (iplugin->length - 2 * border) / 100;

    if (iplugin->orientation == ORIENT_HORIZ)
    {
        cairo_rectangle(cr,
            border,
            iplugin->height - border - chargeLevel,
            iplugin->width - border * 2,
            chargeLevel);
    }
    else
    {
        cairo_rectangle(cr,
            border,
            border,
            chargeLevel,
            iplugin->height - border * 2);
    }
    cairo_fill(cr);

    cairo_destroy(cr);

    gtk_widget_queue_draw(iplugin->drawingArea);
}

void update_label(BatteryPlugin *iplugin) {

    if (!iplugin->label)
        return;

    if (!iplugin->b)
        return;


    gboolean isCharging = battery_is_charging(iplugin->b);

    gchar * text = NULL;
    if (isCharging)
        text = g_strdup_printf(_("%d%%↑"), iplugin->b->percentage);
    else
        text = g_strdup_printf(_("%d%%↓"), iplugin->b->percentage);

    double status_color[3];
    get_status_color(iplugin, status_color);
    int color = (((int) (status_color[0] * 255)) << 16) +
                (((int) (status_color[1] * 255)) << 8) +
                 ((int) (status_color[2] * 255));
    
    gchar * markup = g_strdup_printf("<span color=\"#%06x\">%s</span>", color, text);

    gtk_label_set_markup(GTK_LABEL(iplugin->label), markup);

    g_free(markup);
    g_free(text);
}

/* FIXME:
   Don't repaint if percentage of remaining charge and remaining time aren't changed. */
void update_display(BatteryPlugin *iplugin) {
    char tooltip[ 256 ];
    battery *b = iplugin->b;
    /* unit: mW */


    /* no battery is found */
    if( b == NULL ) 
    {
        gtk_widget_set_tooltip_text( iplugin->vbox, _("No batteries found") );

        if (!iplugin->label)
        {
            iplugin->label = gtk_label_new("");
            gtk_box_pack_start(GTK_BOX(iplugin->hbox), iplugin->label, TRUE, FALSE, 0);
        }

        gtk_widget_show(iplugin->label);
        gtk_widget_hide(iplugin->drawingArea);

        gtk_label_set_text(GTK_LABEL(iplugin->label), _("N/A"));

	return;
    }
    
    /* fixme: only one battery supported */

    int rate = iplugin->b->current_now;
    gboolean isCharging = battery_is_charging ( b );
    
    /* Consider running the alarm command */
    if ( !isCharging && rate > 0 &&
	( ( battery_get_remaining( b ) / 60 ) < iplugin->alarmTime ) )
    {
	/* Shrug this should be done using glibs process functions */
	/* Alarms should not run concurrently; determine whether an alarm is
	   already running */
	int alarmCanRun;
	sem_getvalue(&(iplugin->alarmProcessLock), &alarmCanRun);
	
	/* Run the alarm command if it isn't already running */
	if (alarmCanRun) {
	    
	    Alarm *a = (Alarm *) malloc(sizeof(Alarm));
	    a->command = iplugin->alarmCommand;
	    a->lock = &(iplugin->alarmProcessLock);
	    
	    /* Manage the alarm process in a new thread, which which will be
	       responsible for freeing the alarm struct it's given */
	    pthread_t alarmThread;
	    pthread_create(&alarmThread, NULL, alarmProcess, a);
	    
	}
    }

    /* Make a tooltip string, and display remaining charge time if the battery
       is charging or remaining life if it's discharging */
    if (isCharging) {
        if (iplugin->b->seconds >= 60) {
            int hours = iplugin->b->seconds / 3600;
            int left_seconds = b->seconds - 3600 * hours;
            int minutes = left_seconds / 60;
            snprintf(tooltip, 256,
                _("Battery: %d%% charged, %d:%02d until full"),
                iplugin->b->percentage,
                hours,
                minutes );
        }
        else {
            snprintf(tooltip, 256,
                _("Battery: %d%% charged"),
                iplugin->b->percentage);
        }
    } else {
        /* if we have enough rate information for battery */
        if (iplugin->b->percentage != 100) {
            int hours = iplugin->b->seconds / 3600;
            int left_seconds = b->seconds - 3600 * hours;
            int minutes = left_seconds / 60;
            snprintf(tooltip, 256,
                _("Battery: %d%% charged, %d:%02d left"),
                iplugin->b->percentage,
                hours,
            minutes );
        } else {
            snprintf(tooltip, 256,
                _("Battery: %d%% charged"),
                100 );
        }
    }

    if (iplugin->b->voltage_now > 0 && iplugin->b->current_now > 0)
    {
        if (isCharging)
        {
            snprintf(tooltip + strlen(tooltip), 256 - strlen(tooltip),
                _("\nVoltage %.1fV\nCharging current: %.1fA"),
                iplugin->b->voltage_now / 1000.0,
                iplugin->b->current_now / 1000.0
            );
        }
        else
        {
            int power_now = iplugin->b->power_now;
            if (power_now <= 0)
                power_now = iplugin->b->voltage_now * iplugin->b->current_now / 1000;
            snprintf(tooltip + strlen(tooltip), 256 - strlen(tooltip),
                _("\nPower: %.1fW (%.1fV, %.1fA)"),
                power_now / 1000.0,
                iplugin->b->voltage_now / 1000.0,
                iplugin->b->current_now / 1000.0
            );
        }
    }

    gtk_widget_set_tooltip_text(iplugin->vbox, tooltip);

    if (iplugin->display_as == DISPLAY_AS_BAR)
    {
        if (iplugin->label)
            gtk_widget_hide(iplugin->label);
        gtk_widget_show(iplugin->drawingArea);
        update_bar(iplugin);
    }
    else
    {
        if (!iplugin->label)
        {
            iplugin->label = gtk_label_new("");
            gtk_box_pack_start(GTK_BOX(iplugin->hbox), iplugin->label, TRUE, FALSE, 0);
        }
        gtk_widget_show(iplugin->label);
        gtk_widget_hide(iplugin->drawingArea);
        update_label(iplugin);
    }

}

/* This callback is called every 3 seconds */
static int update_timout(BatteryPlugin *iplugin) {
    GDK_THREADS_ENTER();
    iplugin->state_elapsed_time++;
    iplugin->info_elapsed_time++;

    /* check the batteries every 3 seconds */
    if (iplugin->b)
        battery_update( iplugin->b );
    else
        iplugin->b = battery_get();

    update_display(iplugin);

    GDK_THREADS_LEAVE();
    return TRUE;
}

/* An update will be performed whenever the user clicks on the charge bar */
static gint buttonPressEvent(GtkWidget *widget, GdkEventButton *event,
        Plugin* plugin) {

    BatteryPlugin *iplugin = PRIV(plugin);

    update_display(iplugin);

    if (plugin_button_press_event(widget, event, plugin))
        return TRUE;

    return FALSE;
}


static gint configureEvent(GtkWidget *widget, GdkEventConfigure *event,
        BatteryPlugin *iplugin) {

    ENTER;

    if (iplugin->width == widget->allocation.width && iplugin->height == widget->allocation.height)
    {
        RET(TRUE);
    }

    //g_print("allocation: %d, %d\n", widget->allocation.width, widget->allocation.height);

    /* Update the plugin's dimensions */
    iplugin->width = widget->allocation.width;
    iplugin->height = widget->allocation.height;
    if (iplugin->orientation == ORIENT_HORIZ) {
        iplugin->length = iplugin->height;
    }
    else {
        iplugin->length = iplugin->width;
    }

    if (iplugin->pixmap)
        g_object_unref(iplugin->pixmap);

    iplugin->pixmap = gdk_pixmap_new (widget->window, widget->allocation.width,
          widget->allocation.height, -1);

    /* Perform an update so the bar will look right in its new orientation */
    update_display(iplugin);

    RET(TRUE);

}

static void sizeRequest(GtkWidget * widget, GtkRequisition * requisition, BatteryPlugin *iplugin)
{
    requisition->width = iplugin->bar_preferred_width;
    requisition->height = iplugin->bar_preferred_height;
    //g_print("requisition: %d, %d\n", iplugin->bar_preferred_width, iplugin->bar_preferred_height);
}

static gint exposeEvent(GtkWidget *widget, GdkEventExpose *event, BatteryPlugin *iplugin) {

    ENTER;

    gdk_draw_drawable (widget->window, iplugin->drawingArea->style->black_gc,
            iplugin->pixmap, event->area.x, event->area.y, event->area.x,
            event->area.y, event->area.width, event->area.height);

    RET(FALSE);

}


static int
constructor(Plugin *p)
{
    ENTER;

    BatteryPlugin *iplugin;
    iplugin = g_new0(BatteryPlugin, 1);
    plugin_set_priv(p, iplugin);

    /* get available battery */
    iplugin->b = battery_get ();

    /* no battery available */
/*    if ( iplugin->b == NULL )
	goto error;*/

    GtkWidget * pwid = gtk_event_box_new();
    plugin_set_widget(p, pwid);
    gtk_widget_set_has_window(pwid, FALSE);
    gtk_container_set_border_width( GTK_CONTAINER(pwid), 1 );

    iplugin->vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(pwid), iplugin->vbox);
    gtk_widget_show(iplugin->vbox);

    iplugin->hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(iplugin->vbox), iplugin->hbox, TRUE, FALSE, 0);
    gtk_widget_show(iplugin->hbox);

    iplugin->drawingArea = gtk_drawing_area_new();
    gtk_widget_add_events( iplugin->drawingArea, GDK_BUTTON_PRESS_MASK );
    gtk_box_pack_start(GTK_BOX(iplugin->hbox), iplugin->drawingArea, TRUE, FALSE, 0);

    gtk_widget_show(iplugin->drawingArea);

    g_signal_connect (G_OBJECT (pwid), "button_press_event",
            G_CALLBACK(buttonPressEvent), (gpointer) p);
    g_signal_connect (G_OBJECT (iplugin->drawingArea),"configure_event",
          G_CALLBACK (configureEvent), (gpointer) iplugin);
    g_signal_connect (G_OBJECT (iplugin->drawingArea), "size-request",
          G_CALLBACK (sizeRequest), (gpointer) iplugin);
    g_signal_connect (G_OBJECT (iplugin->drawingArea), "expose_event",
          G_CALLBACK (exposeEvent), (gpointer) iplugin);

    sem_init(&(iplugin->alarmProcessLock), 0, 1);

    iplugin->alarmCommand = iplugin->backgroundColor = iplugin->chargingColor1 = iplugin->chargingColor2
            = iplugin->dischargingColor1 = iplugin->dischargingColor2 = NULL;

    /* Set default values for integers */
    iplugin->alarmTime = 5;
    iplugin->border_width = 3;
    iplugin->display_as = DISPLAY_AS_TEXT;
    iplugin->alarmCommand = g_strdup("xmessage Battery low");
    iplugin->backgroundColor = g_strdup("black");
    iplugin->chargingColor1 = g_strdup("#1B3BC6");
    iplugin->chargingColor2 = g_strdup("#B52FC3");
    iplugin->dischargingColor1 = g_strdup("#00FF00");
    iplugin->dischargingColor2 = g_strdup("#FF0000");

    wtl_json_read_options(plugin_inner_json(p), option_definitions, iplugin);

    color_parse_d(iplugin->backgroundColor, iplugin->background_color);
    color_parse_d(iplugin->chargingColor1, iplugin->charging1_color);
    color_parse_d(iplugin->chargingColor2, iplugin->charging2_color);
    color_parse_d(iplugin->dischargingColor1, iplugin->discharging1_color);
    color_parse_d(iplugin->dischargingColor2, iplugin->discharging2_color);

    batt_panel_configuration_changed(p);

    /* Start the update loop */
    iplugin->timer = g_timeout_add_seconds( 3, (GSourceFunc) update_timout, (gpointer) iplugin);

    RET(TRUE);

/*error:
    RET(FALSE);*/
}


static void
destructor(Plugin *p)
{
    ENTER;

    BatteryPlugin * iplugin = PRIV(p);

    if (iplugin->pixmap)
        g_object_unref(iplugin->pixmap);

    g_free(iplugin->alarmCommand);
    g_free(iplugin->backgroundColor);
    g_free(iplugin->chargingColor1);
    g_free(iplugin->chargingColor2);
    g_free(iplugin->dischargingColor1);
    g_free(iplugin->dischargingColor2);

    g_free(iplugin->rateSamples);
    sem_destroy(&(iplugin->alarmProcessLock));
    if (iplugin->timer)
        g_source_remove(iplugin->timer);
    g_free(iplugin);

    RET();

}


static void batt_panel_configuration_changed(Plugin *p) {

    ENTER;

    BatteryPlugin *b = PRIV(p);

    b->orientation = plugin_get_orientation(p);

    if (b->orientation == ORIENT_HORIZ)
    {
        b->bar_preferred_height = plugin_get_icon_size(p);;
        b->bar_preferred_width = b->bar_preferred_height / 3;
        if (b->bar_preferred_width < 5)
            b->bar_preferred_width = 5;
    }
    else
    {
        b->bar_preferred_width = plugin_get_icon_size(p);;
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

    BatteryPlugin *b = PRIV(p);

    /* Update colors */
    color_parse_d(b->backgroundColor, b->background_color);
    color_parse_d(b->chargingColor1, b->charging1_color);
    color_parse_d(b->chargingColor2, b->charging2_color);
    color_parse_d(b->dischargingColor1, b->discharging1_color);
    color_parse_d(b->dischargingColor2, b->discharging2_color);

    /* Make sure the border value is acceptable */
    b->border_width = MAX(0, b->border_width);

    update_display(b);

    RET();
}


static void config(Plugin *p, GtkWindow* parent) {
    ENTER;

    GtkWidget *dialog;
    BatteryPlugin *b = PRIV(p);
    dialog = create_generic_config_dlg(_(plugin_class(p)->name),
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
            _("Border width"), &b->border_width, (GType)CONF_TYPE_INT,
            NULL);
    if (dialog)
        gtk_window_present(GTK_WINDOW(dialog));

    RET();
}


static void save(Plugin* p) {
    BatteryPlugin *iplugin = PRIV(p);
    wtl_json_write_options(plugin_inner_json(p), option_definitions, iplugin);
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
