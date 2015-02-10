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

#include <gtk/gtk.h>
#include <stdlib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>

#include <sde-utils-jansson.h>

#define PLUGIN_PRIV_TYPE netstatus

#include <waterline/panel.h>
#include <waterline/misc.h>
#include <waterline/plugin.h>

#include <waterline/gtkcompat.h>

#include "netstatus-icon.h"
#include "netstatus-dialog.h"

typedef struct {
    Plugin* plugin;
    char *iface;
    char *config_tool;
    GtkWidget *mainw;
    GtkWidget *dlg;
} netstatus;


/******************************************************************************/

#define SU_JSON_OPTION_STRUCTURE netstatus
static su_json_option_definition option_definitions[] = {
    SU_JSON_OPTION(string, iface),
    SU_JSON_OPTION(string, config_tool),
    {0,}
};

/******************************************************************************/


static void
netstatus_destructor(Plugin *p)
{
    netstatus *ns = PRIV(p);
    /* The widget is destroyed in plugin_stop().
    gtk_widget_destroy(ns->mainw);
    */
    g_free( ns->iface );
    g_free( ns->config_tool );
    g_free(ns);
}

static void on_response( GtkDialog* dlg, gint response, netstatus *ns )
{
    const char* iface;
    switch( response )
    {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_NONE:
        iface = netstatus_dialog_get_iface_name((GtkWidget*)dlg);
        if( iface )
        {
            g_free(ns->iface);
            ns->iface = g_strdup(iface);
            gtk_widget_destroy( GTK_WIDGET(dlg) );
            ns->dlg = NULL;
        }
    }
}

static gboolean on_button_press( GtkWidget* widget, GdkEventButton* evt, Plugin* p )
{
    NetstatusIface* iface;
    netstatus *ns = PRIV(p);

    /* Standard right-click handling. */
    if (plugin_button_press_event(widget, evt, p))
        return TRUE;

    if( evt->button == 1 ) /*  Left click*/
    {
        if( ! ns->dlg )
        {
            iface = netstatus_icon_get_iface( NETSTATUS_ICON(widget) );
            ns->dlg = netstatus_dialog_new(iface);
            if ( ns->dlg )
            {
                /* fix background */
                gtk_widget_set_style(ns->dlg, panel_get_default_style(plugin_panel(p)));
                netstatus_dialog_set_configuration_tool( ns->dlg, ns->config_tool );
                g_signal_connect( ns->dlg, "response", G_CALLBACK(on_response), ns );
            }
        }
        if ( ns->dlg )
            gtk_window_present( GTK_WINDOW(ns->dlg) );
    }
    return TRUE;
}

static int
netstatus_constructor(Plugin *p)
{
    netstatus *ns;
    NetstatusIface* iface;

    ns = g_new0(netstatus, 1);
    g_return_val_if_fail(ns != NULL, 0);
    plugin_set_priv(p, ns);
    ns->plugin = p;

    ns->iface = g_strdup("eth0");
    ns->config_tool = g_strdup("network-admin --configure %i");

    su_json_read_options(plugin_inner_json(p), option_definitions, ns);

    iface = netstatus_iface_new(ns->iface);
    ns->mainw = netstatus_icon_new(iface);

    gtk_widget_set_has_window(GTK_WIDGET(ns->mainw), FALSE);

    netstatus_icon_set_show_signal((NetstatusIcon *)ns->mainw, TRUE);
    gtk_widget_add_events( ns->mainw, GDK_BUTTON_PRESS_MASK );
    g_object_unref( iface );
    g_signal_connect( ns->mainw, "button-press-event",
                      G_CALLBACK(on_button_press), p );

    gtk_widget_show(ns->mainw);

    plugin_set_widget(p, ns->mainw);

    return 1;
}

static void apply_config(Plugin* p)
{
    netstatus *ns = PRIV(p);
    NetstatusIface* iface;

    iface = netstatus_iface_new(ns->iface);
    netstatus_icon_set_iface((NetstatusIcon *)ns->mainw, iface);
}

static void netstatus_config( Plugin* p, GtkWindow* parent  )
{
    netstatus * ns = PRIV(p);
    GtkWidget * dialog = create_generic_config_dialog(
                _(plugin_class(p)->name),
                GTK_WIDGET(parent),
                (GSourceFunc) apply_config, p,
                _("Interface to monitor"), &ns->iface, (GType)CONF_TYPE_STR,
                _("Config tool"), &ns->config_tool, (GType)CONF_TYPE_STR,
                NULL );
    if (dialog)
        gtk_window_present(GTK_WINDOW(dialog));
}

static void save_config( Plugin* p)
{
    netstatus *ns = PRIV(p);
    su_json_write_options(plugin_inner_json(p), option_definitions, ns);
}

PluginClass netstatus_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "netstatus",
    name : N_("Network Status Monitor"),
    version: "1.0",
    description : N_("Monitor network status"),
    category: PLUGIN_CATEGORY_HW_INDICATOR,

    /* Reloading netstatus results in segfault due to registering static type. */
    not_unloadable : TRUE,

    constructor : netstatus_constructor,
    destructor  : netstatus_destructor,
    config : netstatus_config,
    save : save_config
};
