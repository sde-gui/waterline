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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>

#include <sde-utils.h>
#include <sde-utils-jansson.h>

#define PLUGIN_PRIV_TYPE WinCmdPlugin

#include <waterline/symbol_visibility.h>
#include <waterline/panel.h>
#include <waterline/misc.h>
#include <waterline/fb_button.h>
#include <waterline/plugin.h>
#include <waterline/Xsupport.h>

/* Commands that can be issued. */
typedef enum {
    WC_NONE,
    WC_ICONIFY,
    WC_SHADE
} WindowCommand;

/* Private context for window command plugin. */
typedef struct {
    char * image;				/* Main icon */
    gboolean toggle_state;			/* State of toggle */
} WinCmdPlugin;

static void wincmd_execute(WinCmdPlugin * wc, WindowCommand command);
static gboolean wincmd_button_clicked(GtkWidget * widget, GdkEventButton * event, Plugin * plugin);
static int wincmd_constructor(Plugin * p);
static void wincmd_destructor(Plugin * p);
static void wincmd_save_configuration(Plugin * p);
static void wincmd_panel_configuration_changed(Plugin * p);

/******************************************************************************/

#define SU_JSON_OPTION_STRUCTURE WinCmdPlugin
static su_json_option_definition option_definitions[] = {
    SU_JSON_OPTION(string, image),
    {0,}
};

/******************************************************************************/

/* Execute a window command. */
static void wincmd_execute(WinCmdPlugin * wc, WindowCommand command)
{
    /* Get the list of all windows. */
    int client_count;
    Window * client_list = wtl_x11_get_xa_property (GDK_ROOT_WINDOW(), a_NET_CLIENT_LIST, XA_WINDOW, &client_count);
    if (client_list != NULL)
    {
        /* Loop over all windows. */
        guint current_desktop = get_net_current_desktop();
        int i;
        for (i = 0; i < client_count; i++)
        {
            /* Get the desktop and window type properties. */
            NetWMWindowType nwwt;
            guint task_desktop = get_net_wm_desktop(client_list[i]);
            get_net_wm_window_type(client_list[i], &nwwt);

            /* If the task is visible on the current desktop and it is an ordinary window,
             * execute the requested Iconify or Shade change. */
            if (((task_desktop == -1) || (task_desktop == current_desktop))
            && (( ! nwwt.dock) && ( ! nwwt.desktop) && ( ! nwwt.splash)))
            {
                switch (command)
                {
                    case WC_NONE:
                        break;

                    case WC_ICONIFY:
                        if (! wc->toggle_state)
                            XIconifyWindow(GDK_DISPLAY(), client_list[i], DefaultScreen(GDK_DISPLAY()));
                        else
                            XMapWindow (GDK_DISPLAY(), client_list[i]);
                        break;

                    case WC_SHADE:
                        Xclimsg(client_list[i], a_NET_WM_STATE,
                            ((( ! wc->toggle_state)) ? a_NET_WM_STATE_ADD : a_NET_WM_STATE_REMOVE),
                            a_NET_WM_STATE_SHADED, 0, 0, 0);
                        break;
                }
            }
        }
        XFree(client_list);

        /* Adjust toggle state. */
        wc->toggle_state = !wc->toggle_state;
    }
}

/* Handler for "clicked" signal on main widget. */
static gboolean wincmd_button_clicked(GtkWidget * widget, GdkEventButton * event, Plugin * plugin)
{
    WinCmdPlugin * wc = PRIV(plugin);

    /* Standard right-click handling. */
    if (plugin_button_press_event(widget, event, plugin))
        return TRUE;

    /* Left-click to iconify. */
    if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
        GdkScreen* screen = gtk_widget_get_screen(widget);
        static GdkAtom atom = 0;
        if( G_UNLIKELY(0 == atom) )
            atom = gdk_atom_intern("_NET_SHOWING_DESKTOP", FALSE);

        /* If window manager supports _NET_SHOWING_DESKTOP, use it.
         * Otherwise, fall back to iconifying windows individually. */
        if (gdk_x11_screen_supports_net_wm_hint(screen, atom))
        {
            int showing_desktop = (( ( ! wc->toggle_state)) ? 1 : 0);
            Xclimsg(DefaultRootWindow(GDK_DISPLAY()), a_NET_SHOWING_DESKTOP, showing_desktop, 0, 0, 0, 0);
            wc->toggle_state = !wc->toggle_state;
        }
        else
            wincmd_execute(wc, WC_ICONIFY);
    }

    /* Middle-click to shade. */
    else if (event->button == 2)
        wincmd_execute(wc, WC_SHADE);

    return TRUE;
}

/* Plugin constructor. */
static int wincmd_constructor(Plugin * p)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    WinCmdPlugin * wc = g_new0(WinCmdPlugin, 1);
    plugin_set_priv(p, wc);

    su_json_read_options(plugin_inner_json(p), option_definitions, wc);

    /* Default image. */
    if (su_str_empty(wc->image))
    {
        g_free(wc->image);
        wc->image = g_strdup("window-manager");
    }

    /* Allocate top level widget and set into Plugin widget pointer. */
    GtkWidget * pwid = fb_button_new_from_file(wc->image, plugin_get_icon_size(p), plugin_get_icon_size(p), p);
    plugin_set_widget(p, pwid);
    gtk_container_set_border_width(GTK_CONTAINER(pwid), 0);
    g_signal_connect(G_OBJECT(pwid), "button_press_event", G_CALLBACK(wincmd_button_clicked), (gpointer) p);
    gtk_widget_set_tooltip_text(pwid, _("Left click to iconify all windows.  Middle click to shade them."));

    /* Show the widget and return. */
    gtk_widget_show(pwid);
    return 1;
}

/* Plugin destructor. */
static void wincmd_destructor(Plugin * p)
{
    WinCmdPlugin * wc = PRIV(p);
    g_free(wc->image);
    g_free(wc);
}


/* Save the configuration to the configuration file. */
static void wincmd_save_configuration(Plugin * p)
{
    WinCmdPlugin * wc = PRIV(p);
    su_json_write_options(plugin_inner_json(p), option_definitions, wc);
}

/* Callback when panel configuration changes. */
static void wincmd_panel_configuration_changed(Plugin * p)
{
    WinCmdPlugin * wc = PRIV(p);
    fb_button_set_from_file(plugin_widget(p), wc->image, plugin_get_icon_size(p), plugin_get_icon_size(p));
}

/* Plugin descriptor. */
SYMBOL_PLUGIN_CLASS PluginClass wincmd_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "wincmd",
    name : N_("Minimize All Windows"),
    version: "1.0",
    description : N_("Sends commands to all desktop windows.\nSupported commands are 1) iconify and 2) shade"),
    category: PLUGIN_CATEGORY_WINDOW_MANAGEMENT,

    constructor : wincmd_constructor,
    destructor  : wincmd_destructor,
    save : wincmd_save_configuration,
    panel_configuration_changed : wincmd_panel_configuration_changed

};
