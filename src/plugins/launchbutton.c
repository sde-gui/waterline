/**
 * Copyright (c) 2006 LxDE Developers, see the file AUTHORS for details.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <glib/gi18n.h>

//#include <menu-cache.h>

//#include <sys/types.h>
//#include <sys/stat.h>
//#include <unistd.h>
//#include <fcntl.h>

#include "panel.h"
#include "misc.h"
//#include "plugin.h"
//#include "bg.h"
//#include "menu-policy.h"

#include "dbg.h"

typedef struct {
    char * icon_path;
    char * title;
    char * tooltip;
    char * command1;
    char * command2;

    GtkWidget * button;
    GtkWidget * img;
    GtkWidget * label;

    Plugin * plug;
} lb_t;

static void lb_run_command(const char* command) {

    if (!command)
        return;

    while (*command == ' ' || *command == '\t')
        command++;

    int use_terminal = FALSE;

    if (*command == '&')
        use_terminal = TRUE,
        command++;

    lxpanel_launch_app(command, NULL, use_terminal);
}


/* Handler for "button-press-event" event from launch button. */
static gboolean lb_press_event(GtkWidget * widget, GdkEventButton * event, lb_t * lb)
{
    /* Standard right-click handling. */
    if (plugin_button_press_event(widget, event, lb->plug))
        return TRUE;

//plugin_popup_set_position_helper

    if (event->button == 1)    /* left button */
    {
       lb_run_command(lb->command1);
    }
    else if (event->button == 2)    /* middle button */
    {
       lb_run_command(lb->command2);
    }
    return TRUE;
}


/* Callback when the configuration dialog has recorded a configuration change. */
static void lb_apply_configuration(Plugin * p)
{
    lb_t * lb = (lb_t *) p->priv;

    if (!p->pwid)
        p->pwid = gtk_event_box_new(),
        gtk_widget_show(p->pwid);

    if (lb->button)
        gtk_widget_destroy(lb->button),
        lb->button = NULL;


    lb->button = fb_button_new_from_file_with_label(lb->icon_path, p->panel->icon_size, p->panel->icon_size, PANEL_ICON_HIGHLIGHT, TRUE, p->panel, lb->title);
    gtk_container_add(GTK_CONTAINER(p->pwid), lb->button);
    g_signal_connect(lb->button, "button-press-event", G_CALLBACK(lb_press_event), (gpointer) lb);
    gtk_widget_show(lb->button);

    gtk_widget_set_tooltip_text(lb->button, lb->tooltip);
}


/* Plugin constructor. */
static int lb_constructor(Plugin *p, char **fp)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    lb_t * lb = g_new0(lb_t, 1);
    lb->plug = p;
    p->priv = lb;

    lb->icon_path = NULL;
    lb->title     = NULL;
    lb->tooltip   = NULL;
    lb->command1  = NULL;
    lb->command2  = NULL;

    lb->button = NULL;
    lb->img    = NULL;
    lb->label  = NULL;

    /* Load parameters from the configuration file. */
    line s;
    s.len = 256;
    if (fp)
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END)
        {
            if (s.type == LINE_NONE)
            {
                ERR( "launchbutton: illegal token %s\n", s.str);
                return 0;
            }
            if (s.type == LINE_VAR)
            {
                if (g_ascii_strcasecmp(s.t[0], "IconPath") == 0)
                    lb->icon_path = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "Title") == 0)
                    lb->title = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "Tooltip") == 0)
                    lb->tooltip = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "Command1") == 0)
                    lb->command1 = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "Command2") == 0)
                    lb->command2 = g_strdup(s.t[1]);
                else
                    ERR( "dclock: unknown var %s\n", s.t[0]);
            }
            else
            {
                ERR( "dclock: illegal in this context %s\n", s.str);
                return 0;
            }
        }

    }

    #define DEFAULT_STRING(f, v) \
      if (lb->f == NULL) \
          lb->f = g_strdup(v);

    DEFAULT_STRING(icon_path, PACKAGE_DATA_DIR "/lxpanel/images/my-computer.png");
    DEFAULT_STRING(title    , "");
    DEFAULT_STRING(tooltip  , "");
    DEFAULT_STRING(command1 , "");
    DEFAULT_STRING(command2 , "");

    #undef DEFAULT_STRING

    lb_apply_configuration(p);

    return 1;
}


/* Plugin destructor. */
static void lb_destructor(Plugin * p)
{
    lb_t * lb = (lb_t *) p->priv;

    /* Deallocate all memory. */
    g_free(lb->icon_path);
    g_free(lb->title);
    g_free(lb->tooltip);
    g_free(lb->command1);
    g_free(lb->command2);
    g_free(lb);
}


/* Callback when the configuration dialog is to be shown. */
static void lb_configure(Plugin * p, GtkWindow * parent)
{
    lb_t * lb = (lb_t *) p->priv;
    GtkWidget * dlg = create_generic_config_dlg(
        _(p->class->name),
        GTK_WIDGET(parent),
        (GSourceFunc) lb_apply_configuration, (gpointer) p,
        _("Title"), &lb->title, CONF_TYPE_STR,
        _("Tooltip"), &lb->tooltip, CONF_TYPE_STR,
        _("Icon"), &lb->icon_path, CONF_TYPE_FILE_ENTRY,
        _("Left button command"), &lb->command1, CONF_TYPE_STR,
        _("Right button command"), &lb->command2, CONF_TYPE_STR,
        NULL);
    if (dlg)
        gtk_window_present(GTK_WINDOW(dlg));
}


/* Callback when the configuration is to be saved. */
static void lb_save_configuration(Plugin * p, FILE * fp)
{
    lb_t * lb = (lb_t *) p->priv;
    lxpanel_put_str(fp, "IconPath", lb->icon_path);
    lxpanel_put_str(fp, "Title", lb->title);
    lxpanel_put_str(fp, "Tooltip", lb->tooltip);
    lxpanel_put_str(fp, "Command1", lb->command1);
    lxpanel_put_str(fp, "Command2", lb->command2);
}


/* Callback when panel configuration changes. */
static void lb_panel_configuration_changed(Plugin * p)
{
    lb_apply_configuration(p);
}


PluginClass launchbutton_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "launchbutton",
    name : N_("Button"),
    version: "0.1",
    description : N_("Launch button"),

    constructor : lb_constructor,
    destructor  : lb_destructor,
    config : lb_configure,
    save : lb_save_configuration,
    panel_configuration_changed : lb_panel_configuration_changed
};

