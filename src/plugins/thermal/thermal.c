/**
 * Thermal plugin
 *
 * Copyright (C) 2007 by Daniel Kesler <kesler.daniel@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <sde-utils-jansson.h>
#include <string.h>

#define PLUGIN_PRIV_TYPE thermal

#include <waterline/panel.h>
#include <waterline/misc.h>
#include <waterline/plugin.h>
#include <waterline/gtkcompat.h>

#define SYSFS_THERMAL_DIRECTORY "/sys/class/thermal/" /* must be slash-terminated */
#define SYSFS_THERMAL_SUBDIR_PREFIX "thermal_zone"
#define SYSFS_THERMAL_TEMPF  "temp"
#define SYSFS_THERMAL_TRIP  "trip_point_0_temp"


typedef struct _thermal {
    Plugin * plugin;
    GtkWidget *main;
    GtkWidget *namew;
    int previous_temperature;
    int critical;
    int warning1_temperature;
    int warning2_temperature;
    gboolean autoselect_warning_levels, autoselect_sensor;
    char *sensor,
         *normal_color,
         *warning1_color,
         *warning2_color;
    unsigned int timer;
    GdkColor cl_normal,
             cl_warning1,
             cl_warning2;
    gint (*get_temperature)(struct _thermal *th);
    gint (*get_critical)(struct _thermal *th);
} thermal;

/******************************************************************************/

#define SU_JSON_OPTION_STRUCTURE thermal
static su_json_option_definition option_definitions[] = {
    SU_JSON_OPTION(string, normal_color),
    SU_JSON_OPTION(string, warning1_color),
    SU_JSON_OPTION(string, warning2_color),
    SU_JSON_OPTION(bool, autoselect_warning_levels),
    SU_JSON_OPTION(int, warning1_temperature),
    SU_JSON_OPTION(int, warning2_temperature),
    SU_JSON_OPTION(bool, autoselect_sensor),
    SU_JSON_OPTION(string, sensor),
    {0,}
};

/******************************************************************************/

 static gint
sysfs_get_critical(thermal *th){
    FILE *state;
    char buf[ 256 ], sstmp [ 100 ];
    char* pstr = NULL;

    if(th->sensor == NULL) return -1;

    sprintf(sstmp,"%s%s",th->sensor,SYSFS_THERMAL_TRIP);

    if (!(state = fopen( sstmp, "r"))) {
        //printf("cannot open %s\n",sstmp);
        return -1;
    }

    while( fgets(buf, 256, state) &&
            ! ( pstr = buf ) );
    if( pstr )
    {
        fclose(state);
        return atoi(pstr)/1000;
    }

    fclose(state);
    return -1;
}

static gint
sysfs_get_temperature(thermal *th){
    FILE *state;
    char buf[ 256 ], sstmp [ 100 ];
    char* pstr = NULL;

    if(th->sensor == NULL) return -1;

    sprintf(sstmp,"%s%s",th->sensor,SYSFS_THERMAL_TEMPF);

    if (!(state = fopen( sstmp, "r"))) {
        //printf("cannot open %s\n",sstmp);
        return -1;
    }

    while (fgets(buf, 256, state) &&
          ! ( pstr = buf ) );
    if( pstr )
    {
        fclose(state);
        return atoi(pstr)/1000;
    }

    fclose(state);
    return -1;
}


static void
set_get_functions(thermal *th)
{
    th->get_temperature = sysfs_get_temperature;
    th->get_critical = sysfs_get_critical;
}

static void update_display(thermal *th, gboolean force)
{
    int temp = th->get_temperature(th);

    if (th->previous_temperature == temp && !force)
        return;

    th->previous_temperature = temp;

    GdkColor color;

    if (temp >= th->warning2_temperature)
        color = th->cl_warning2;
    else if (temp >= th->warning1_temperature)
        color = th->cl_warning1;
    else
        color = th->cl_normal;

    if (temp == -1)
    {
        panel_draw_label_text(plugin_panel(th->plugin), th->namew, "N/A", STYLE_BOLD | STYLE_CUSTOM_COLOR);
    }
    else
    {
        gchar * buffer = g_strdup_printf("<span color=\"#%06x\"><b>%02d°C</b></span>",
            wtl_util_gdkcolor_to_uint32(&color), temp);
        gtk_label_set_markup (GTK_LABEL(th->namew), buffer);
        g_free(buffer);
    }
}

static gboolean update_display_timeout(thermal *th)
{
    update_display(th, FALSE);
    return TRUE;
}

/* FIXME: поменять здесь везде работу с char[] и sprintf на gchar* и g_strdup_printf. */


/* get_sensor():
 *      - Get the sensor directory, and store it in '*sensor'.
 *      - It is searched for in 'directory'.
 *      - Only the subdirectories starting with 'subdir_prefix' are accepted as sensors.
 *      - 'subdir_prefix' may be NULL, in which case any subdir is considered a sensor. */
static void
get_sensor(char** sensor, char const* directory, char const* subdir_prefix)
 {
     GDir *sensorsDirectory;
     const char *sensor_name;
     char sensor_path[100];

    if (! (sensorsDirectory = g_dir_open(directory, 0, NULL)))
    {
        *sensor = NULL;
        return;
    }

    /* Scan the thermal_zone directory for available sensors */
    while ((sensor_name = g_dir_read_name(sensorsDirectory))) {
        if (sensor_name[0] != '.') {
            if (subdir_prefix) {
                if (strncmp(sensor_name, subdir_prefix, strlen(subdir_prefix)) != 0)
                    continue;
            }
            sprintf(sensor_path,"%s%s/", directory, sensor_name);
            if(*sensor) {
                g_free(*sensor);
                *sensor = NULL;
            }
            *sensor = strdup(sensor_path);
            break;
        }
    }
    g_dir_close(sensorsDirectory);
}

static void
check_sensors( thermal *th )
{
    if(th->sensor) {
        g_free(th->sensor);
        th->sensor = NULL;
    }

    get_sensor(&th->sensor, SYSFS_THERMAL_DIRECTORY, SYSFS_THERMAL_SUBDIR_PREFIX);

    //printf("thermal sensor: %s\n", th->sensor);
}

static void
sensor_changed(thermal *th)
{
    //if (th->sensor == NULL) th->auto_sensor = TRUE;
    if (th->autoselect_sensor) check_sensors(th);

    set_get_functions(th);

    th->critical = th->get_critical(th);

    if (th->autoselect_warning_levels && th->critical > 0) {
        th->warning1_temperature = th->critical - 10;
        th->warning2_temperature = th->critical - 5;
    }
}

static int
thermal_constructor(Plugin *p)
{
    thermal *th;

    th = g_new0(thermal, 1);
    th->plugin = p;
    plugin_set_priv(p, th);

    GtkWidget * pwid = gtk_event_box_new();
    plugin_set_widget(p, pwid);
    gtk_widget_set_has_window(pwid, FALSE);
    gtk_container_set_border_width( GTK_CONTAINER(pwid), 2 );

    th->namew = gtk_label_new("ww");
    gtk_container_add(GTK_CONTAINER(pwid), th->namew);

    th->main = pwid;

    g_signal_connect (G_OBJECT (pwid), "button_press_event",
          G_CALLBACK (plugin_button_press_event), (gpointer) p);

    th->warning1_temperature = 75;
    th->warning2_temperature = 80;
    th->autoselect_warning_levels = TRUE;
    th->autoselect_sensor = TRUE;

    th->normal_color = g_strdup("#00ff00");
    th->warning1_color = g_strdup("#fff000");
    th->warning2_color = g_strdup("#ff0000");

    su_json_read_options(plugin_inner_json(p), option_definitions, th);

    gdk_color_parse(th->normal_color,   &(th->cl_normal));
    gdk_color_parse(th->warning1_color, &(th->cl_warning1));
    gdk_color_parse(th->warning2_color, &(th->cl_warning2));


    sensor_changed(th);

    gtk_widget_show(th->namew);

    update_display(th, TRUE);
    th->timer = g_timeout_add(2000, (GSourceFunc) update_display_timeout, (gpointer)th);

    return TRUE;
}

static void applyConfig(Plugin* p)
{
    thermal *th = PRIV(p);

    if (th->normal_color) gdk_color_parse(th->normal_color, &th->cl_normal);
    if (th->warning1_color) gdk_color_parse(th->warning1_color, &th->cl_warning1);
    if (th->warning2_color) gdk_color_parse(th->warning2_color, &th->cl_warning2);

    sensor_changed(th);
}

static void config(Plugin *p, GtkWindow* parent)
{
    GtkWidget *dialog;
    thermal *th = PRIV(p);
    dialog = wtl_create_generic_config_dialog(_(plugin_class(p)->name),
            GTK_WIDGET(parent),
            (GSourceFunc) applyConfig, (gpointer) p,
            "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
            _("Colors"), 0, (GType)CONF_TYPE_TITLE,
            _("Normal"), &th->normal_color, (GType)CONF_TYPE_COLOR,
            _("Warning1"), &th->warning1_color, (GType)CONF_TYPE_COLOR,
            _("Warning2"), &th->warning2_color, (GType)CONF_TYPE_COLOR,
            "", 0, (GType)CONF_TYPE_END_TABLE,

            _("Automatic sensor location"), &th->autoselect_sensor, (GType)CONF_TYPE_BOOL,
            _("Sensor"), &th->sensor, (GType)CONF_TYPE_STR,
            _("Automatic temperature levels"), &th->autoselect_warning_levels, (GType)CONF_TYPE_BOOL,
            _("Warning1 Temperature"), &th->warning1_temperature, (GType)CONF_TYPE_INT,
            _("Warning2 Temperature"), &th->warning2_temperature, (GType)CONF_TYPE_INT,
            NULL);
    if (dialog)
        gtk_window_present(GTK_WINDOW(dialog));
}

static void
thermal_destructor(Plugin *p)
{
  thermal *th = PRIV(p);

  g_free(th->sensor);
  g_free(th->normal_color);
  g_free(th->warning1_color);
  g_free(th->warning2_color);
  g_source_remove(th->timer);
  g_free(th);
}

static void save_config( Plugin* p)
{
    thermal *th = (thermal *)PRIV(p);
    su_json_write_options(plugin_inner_json(p), option_definitions, th);
}

PluginClass thermal_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "thermal",
    name : N_("Temperature Monitor"),
    version: VERSION,
    description : N_("Displays the system temperature"),
    category: PLUGIN_CATEGORY_HW_INDICATOR,

    constructor : thermal_constructor,
    destructor  : thermal_destructor,
    show_properties : config,
    save_configuration : save_config,
};
