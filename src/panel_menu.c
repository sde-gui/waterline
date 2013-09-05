/**
 * Copyright (c) 2011-2013 Vadim Ushakov
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include <waterline/global.h>
#include "plugin_internal.h"
#include "plugin_private.h"
#include <waterline/paths.h>
#include <waterline/panel.h>
#include "panel_internal.h"
#include "panel_private.h"
#include <waterline/misc.h>
#include "bg.h"
#include <waterline/Xsupport.h>

#include <waterline/dbg.h>

#include <waterline/gtkcompat.h>

/******************************************************************************/

static gint
panel_popupmenu_configure(GtkWidget *widget, gpointer user_data)
{
    panel_configure( (Panel*)user_data, 0 );
    return TRUE;
}

static void panel_popupmenu_config_plugin( GtkMenuItem* item, Plugin* plugin )
{
    plugin->class->config( plugin, GTK_WINDOW(plugin->panel->topgwin) );

    /* FIXME: this should be more elegant */
    plugin->panel->config_changed = TRUE;
}

static void panel_popupmenu_add_item( GtkMenuItem* item, Panel* panel )
{
    /* panel_add_plugin( panel, panel->topgwin ); */
    panel_configure( panel, 2 );
}

static void panel_popupmenu_remove_item( GtkMenuItem* item, Plugin* plugin )
{
    Panel* panel = plugin->panel;

    gboolean ok = TRUE;

    GtkWidget* dlg;

    dlg = gtk_message_dialog_new_with_markup(GTK_WINDOW(panel->topgwin),
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_QUESTION,
                                             GTK_BUTTONS_OK_CANCEL,
                                             _("Really delete plugin \"%s\" from the panel?"),
                                             _(plugin->class->name));

    panel_apply_icon(GTK_WINDOW(dlg));
    gtk_window_set_title(GTK_WINDOW(dlg), _("Confirm") );
    ok = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK;
    gtk_widget_destroy( dlg );

    if (!ok)
        return;


    if (panel->pref_dialog.pref_dialog != NULL)
    {
        configurator_remove_plugin_from_list(panel, plugin);
    }

    panel->plugins = g_list_remove( panel->plugins, plugin );
    plugin_delete(plugin);
    panel_save_configuration(panel);
}

static void panel_popupmenu_create_panel( GtkMenuItem* item, Panel* panel )
{
    create_empty_panel();
}

static void panel_popupmenu_delete_panel( GtkMenuItem* item, Panel* panel )
{
    gboolean ok = TRUE;

    if (panel->plugins)
    {
        GtkWidget* dlg;

        dlg = gtk_message_dialog_new_with_markup( GTK_WINDOW(panel->topgwin),
                                                  GTK_DIALOG_MODAL,
                                                  GTK_MESSAGE_QUESTION,
                                                  GTK_BUTTONS_OK_CANCEL,
                                                  _("Really delete this panel?\n<b>Warning: This can not be recovered.</b>") );
        panel_apply_icon(GTK_WINDOW(dlg));
        gtk_window_set_title( (GtkWindow*)dlg, _("Confirm") );
        ok = ( gtk_dialog_run( (GtkDialog*)dlg ) == GTK_RESPONSE_OK );
        gtk_widget_destroy( dlg );
    }

    if (ok)
        delete_panel(panel);
}

static void panel_popupmenu_about( GtkMenuItem* item, Panel* panel )
{
    GtkWidget *about;
    const gchar* authors[] = {
        "Vadim Ushakov (geekless) <igeekless@gmail.com>",
        "Hong Jen Yee (PCMan) <pcman.tw@gmail.com>",
        "Jim Huang <jserv.tw@gmail.com>",
        "Greg McNew <gmcnew@gmail.com> (battery plugin)",
        "Fred Chien <cfsghost@gmail.com>",
        "Daniel Kesler <kesler.daniel@gmail.com>",
        "Juergen Hoetzel <juergen@archlinux.org>",
        "Marty Jack <martyj19@comcast.net>",
        NULL
    };
    /* TRANSLATORS: Replace this string with your names, one name per line. */
    gchar *translators = _("translator-credits");

    gchar * logo_path = wtl_resolve_own_resource("", "images", "my-computer.png", 0);

    about = gtk_about_dialog_new();
    panel_apply_icon(GTK_WINDOW(about));
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), VERSION);
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about), _("Waterline"));
    gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(about), gdk_pixbuf_new_from_file(logo_path, NULL));
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about), _("Copyright (C) 2008-2013"));
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about), _("A lightweight framework for desktop widgets and applets"));
    gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about), __license);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about), __website);
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about), authors);
    gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(about), translators);
    gtk_dialog_run(GTK_DIALOG(about));
    gtk_widget_destroy(about);

    g_free(logo_path);
}

static void panel_popupmenu_quit( GtkMenuItem* item, Panel* panel )
{
    gtk_main_quit();
}

GtkMenu * panel_get_panel_menu(Panel * panel, Plugin * plugin, gboolean use_sub_menu)
{
    GtkWidget  *menu_item, *img;
    GtkMenuShell *ret,*menu;

    char* tmp;
    ret = menu = GTK_MENU_SHELL(gtk_menu_new());

    GtkMenuShell * panel_submenu = GTK_MENU_SHELL(gtk_menu_new());

    menu_item = gtk_menu_item_new_with_mnemonic(_("Pa_nel"));
    gtk_menu_shell_append(menu, menu_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), GTK_WIDGET(panel_submenu) );


    gboolean display_icons = FALSE;

    if (!wtl_is_in_kiosk_mode())
    {

        /*********************/

        if( plugin )
        {
            menu_item = gtk_separator_menu_item_new();
            gtk_menu_shell_prepend(menu, menu_item);

            if (display_icons)
                img = gtk_image_new_from_stock( GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU );

            menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Properties"));

            tmp = g_strdup_printf( _("\"%s\" Settings"), _(plugin->class->name) );
            gtk_widget_set_tooltip_text(GTK_WIDGET(menu_item), tmp);
            g_free( tmp );

            if (display_icons)
                gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );

            gtk_menu_shell_prepend(menu, menu_item);
            if( plugin->class->config )
                g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_config_plugin), plugin );
            else
                gtk_widget_set_sensitive( menu_item, FALSE );
        }

        /*********************/

        if (display_icons)
            img = gtk_image_new_from_stock( GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU );

        menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Add to Panel..."));

        if (display_icons)
            gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );

        gtk_menu_shell_append(menu, menu_item);
        g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_add_item), panel );

        /*********************/

        if( plugin )
        {
            if (display_icons)
                img = gtk_image_new_from_stock( GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU );

            menu_item = gtk_image_menu_item_new_with_mnemonic( _("_Remove From Panel") );

            tmp = g_strdup_printf( _("Remove \"%s\" From Panel"), _(plugin->class->name) );
            gtk_widget_set_tooltip_text(GTK_WIDGET(menu_item), tmp);
            g_free( tmp );

            if (display_icons)
                gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );

            gtk_menu_shell_append(menu, menu_item);
            g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_remove_item), plugin );
        }
/*
        menu_item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(menu, menu_item);
*/


        if (display_icons)
            img = gtk_image_new_from_stock( GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU );
        menu_item = gtk_image_menu_item_new_with_mnemonic(_("Panel _Settings..."));

        tmp = g_strdup_printf( _("Edit settings of panel \"%s\""), panel->name);
        gtk_widget_set_tooltip_text(GTK_WIDGET(menu_item), tmp);
        g_free( tmp );

        if (display_icons)
            gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
        gtk_menu_shell_append(panel_submenu, menu_item);
        g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(panel_popupmenu_configure), panel );

        if (display_icons)
            img = gtk_image_new_from_stock( GTK_STOCK_NEW, GTK_ICON_SIZE_MENU );
        menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Create New Panel"));
        if (display_icons)
            gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
        gtk_menu_shell_append(panel_submenu, menu_item);
        g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_create_panel), panel );

        if (display_icons)
            img = gtk_image_new_from_stock( GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU );
        menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Delete This Panel"));
        if (display_icons)
            gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
        gtk_menu_shell_append(panel_submenu, menu_item);
        g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_delete_panel), panel );
        gtk_widget_set_sensitive(menu_item, panel_count() > 1);

        menu_item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(panel_submenu, menu_item);

    }

    if (display_icons)
        img = gtk_image_new_from_stock( GTK_STOCK_ABOUT, GTK_ICON_SIZE_MENU );
    menu_item = gtk_image_menu_item_new_with_mnemonic(_("A_bout"));
    if (display_icons)
        gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
    gtk_menu_shell_append(panel_submenu, menu_item);
    g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_about), panel );

    if (quit_in_menu)
    {
        menu_item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(menu, menu_item);

        if (display_icons)
            img = gtk_image_new_from_stock( GTK_STOCK_QUIT, GTK_ICON_SIZE_MENU );
        menu_item = gtk_image_menu_item_new_with_label(_("Quit"));
        if (display_icons)
            gtk_image_menu_item_set_image( (GtkImageMenuItem*)menu_item, img );
        gtk_menu_shell_append(panel_submenu, menu_item);
        g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_quit), panel );
    }
/*
    if( use_sub_menu )
    {
        ret = GTK_MENU(gtk_menu_new());
        menu_item = gtk_image_menu_item_new_with_label(_("Panel"));
        gtk_menu_shell_append(GTK_MENU_SHELL(ret), menu_item);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), GTK_WIDGET(menu) );
    }
*/
    gtk_widget_show_all(GTK_WIDGET(ret));

    g_signal_connect( ret, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL );

    if (plugin && plugin_class(plugin)->popup_menu_hook)
        plugin_class(plugin)->popup_menu_hook(plugin, GTK_MENU(ret));

    return GTK_MENU(ret);
}


void panel_show_panel_menu(Panel * panel, Plugin * plugin, GdkEventButton * event)
{
    GtkMenu* popup = panel_get_panel_menu(panel, plugin, FALSE);
    if (popup)
        gtk_menu_popup(popup, NULL, NULL, NULL, NULL, event->button, event->time);
}
