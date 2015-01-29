/**
 * Copyright (c) 2010 LxDE Developers, see the file AUTHORS for details.
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

/* Originally derived from xfce4-xkb-plugin, Copyright 2004 Alexander Iliev,
 * which credits Michael Glickman. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sde-utils-jansson.h>

#include "xkb.h"

/******************************************************************************/

#define SU_JSON_OPTION_STRUCTURE xkb_groups_t
static su_json_option_definition option_definitions[] = {
    SU_JSON_OPTION(bool, display_as_text),
    SU_JSON_OPTION(bool, per_window_layout),
    SU_JSON_OPTION(int, default_group),
    {0,}
};

/******************************************************************************/


static void xkb_groups_active_window_event(FbEv * ev, gpointer data);
static gboolean xkb_groups_scroll_event(GtkWidget * widget, GdkEventScroll * event, gpointer data);
static gboolean xkb_groups_button_press_event(GtkWidget * widget,  GdkEventButton * event, gpointer data);
static int xkb_groups_constructor(Plugin * plugin);
static void xkb_groups_destructor(Plugin * plugin);
static void xkb_groups_display_type_changed(GtkComboBox * cb, gpointer * data);
static void xkb_groups_enable_per_application_changed(GtkToggleButton * tb, gpointer * data);
static void xkb_groups_default_language_changed(GtkComboBox * cb, gpointer * data);
static void xkb_groups_configuration_response(GtkDialog * dialog, gint arg1, gpointer data);
static void xkb_groups_configure(Plugin * p, GtkWindow * parent);
static void xkb_groups_save_configuration(Plugin * p);
static void xkb_groups_panel_configuration_changed(Plugin * p);

/* Redraw the graphics. */
void xkb_groups_update(xkb_groups_t * xkb_groups) 
{
    /* Set the image. */
    gboolean valid_image = FALSE;
    if (!xkb_groups->display_as_text)
    {
        int size = plugin_get_icon_size(xkb_groups->plugin);
        char * group_name = (char *) xkb_groups_get_current_symbol_name_lowercase(xkb_groups);
        if (group_name != NULL)
        {
            gchar * pngname = g_strdup_printf("%s.png", group_name);
            gchar * filename = wtl_resolve_own_resource("", "images", "xkb-flags", pngname, 0);
            GdkPixbuf * unscaled_pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
            g_free(filename);
            g_free(pngname);
            g_free(group_name);

            if (unscaled_pixbuf != NULL)
            {
                /* Loaded successfully. */
                int width = gdk_pixbuf_get_width(unscaled_pixbuf);
                int height = gdk_pixbuf_get_height(unscaled_pixbuf);
                //GdkPixbuf * pixbuf = gdk_pixbuf_scale_simple(unscaled_pixbuf, size * width / height, size, GDK_INTERP_BILINEAR);
                GdkPixbuf * pixbuf = gdk_pixbuf_scale_simple(unscaled_pixbuf, size, size * height / width, GDK_INTERP_BILINEAR);
                if (pixbuf != NULL)
                {
                    gtk_image_set_from_pixbuf(GTK_IMAGE(xkb_groups->image), pixbuf);
                    g_object_unref(G_OBJECT(pixbuf));
                    gtk_widget_hide(xkb_groups->label);
                    gtk_widget_show(xkb_groups->image);
                    gtk_widget_set_tooltip_text(xkb_groups->btn, xkb_groups_get_current_group_name(xkb_groups));
                    gtk_widget_queue_draw(plugin_widget(xkb_groups->plugin));
                    valid_image = TRUE;
                }
                g_object_unref(unscaled_pixbuf);
            }
        }
    }

    /* Set the label. */
    if ((xkb_groups->display_as_text) || ( ! valid_image))
    {
        char * group_name = (char *) xkb_groups_get_current_symbol_name(xkb_groups);
        if (group_name != NULL)
        {
            panel_draw_label_text(plugin_panel(xkb_groups->plugin), xkb_groups->label, (char *) group_name, STYLE_BOLD | STYLE_CUSTOM_COLOR);
            gtk_widget_hide(xkb_groups->image);
            gtk_widget_show(xkb_groups->label);
            gtk_widget_set_tooltip_text(xkb_groups->btn, xkb_groups_get_current_group_name(xkb_groups));
        }
    }
}

/* Handler for "active_window" event on root window listener. */
static void xkb_groups_active_window_event(FbEv * ev, gpointer data) 
{
    xkb_groups_t * xkb_groups = (xkb_groups_t *) data;
    if (xkb_groups->per_window_layout)
    {
        Window * win = fb_ev_active_window(ev);
        if (*win != None)
        {
            xkb_groups_active_window_changed(xkb_groups, *win);
            xkb_groups_update(xkb_groups);
        }
    }
}

/* Handler for "scroll-event" on drawing area. */
static gboolean xkb_groups_scroll_event(GtkWidget * widget, GdkEventScroll * event, gpointer data)
{
    xkb_groups_t * xkb_groups = (xkb_groups_t *) data;

    /* Change to next or previous group. */
    xkb_groups_change_group(xkb_groups,
        (((event->direction == GDK_SCROLL_UP) || (event->direction == GDK_SCROLL_RIGHT)) ? 1 : -1));
    return TRUE;
}

/* Handler for button-press-event on top level widget. */
static gboolean xkb_groups_button_press_event(GtkWidget * widget,  GdkEventButton * event, gpointer data) 
{
    xkb_groups_t * xkb_groups = (xkb_groups_t *) data;

    /* Standard right-click handling. */
    if (plugin_button_press_event(widget, event, xkb_groups->plugin))
        return TRUE;

    /* Change to next group. */
    xkb_groups_change_group(xkb_groups, 1);
    return TRUE;
}

static void xkb_groups_button_enter(GtkWidget * widget, xkb_groups_t * xkb_groups)
{
    gtk_widget_set_state(widget, GTK_STATE_NORMAL);
}

/* Plugin constructor. */
static int xkb_groups_constructor(Plugin * plugin)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    xkb_groups_t * xkb_groups = g_new0(xkb_groups_t, 1);
    xkb_groups->plugin = plugin;
    plugin_set_priv(plugin, xkb_groups);

    /* Initialize to defaults. */
    xkb_groups->display_as_text = FALSE;
    xkb_groups->per_window_layout = TRUE;
    xkb_groups->default_group = 0;

    su_json_read_options(plugin_inner_json(plugin), option_definitions, xkb_groups);

    /* Allocate top level widget and set into Plugin widget pointer. */
    GtkWidget * pwid = gtk_event_box_new();
    plugin_set_widget(plugin, pwid);
    gtk_widget_set_has_window(pwid, FALSE);
    gtk_widget_add_events(pwid, GDK_BUTTON_PRESS_MASK);

    /* Create a button as the child of the event box. */
    xkb_groups->btn = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(pwid), xkb_groups->btn);
    gtk_button_set_relief(GTK_BUTTON(xkb_groups->btn), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(xkb_groups->btn, FALSE);
    gtk_widget_set_can_default(xkb_groups->btn, FALSE);
    gtk_widget_show(xkb_groups->btn);

    /* Create a horizontal box as the child of the button. */
    GtkWidget * hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(xkb_groups->btn), hbox);
    gtk_widget_show(hbox);

    /* Create a label and an image as children of the horizontal box.
     * Only one of these is visible at a time, controlled by user preference
     * and the successful loading of the image. */
    xkb_groups->label = gtk_label_new("");
    gtk_container_add(GTK_CONTAINER(hbox), xkb_groups->label);
    xkb_groups->image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(hbox), xkb_groups->image);

    /* Initialize the XKB interface. */
    xkb_groups_mechanism_constructor(xkb_groups);

    /* Connect signals. */
    g_signal_connect(xkb_groups->btn, "button-press-event", G_CALLBACK(xkb_groups_button_press_event), xkb_groups);
    g_signal_connect(xkb_groups->btn, "scroll-event", G_CALLBACK(xkb_groups_scroll_event), xkb_groups);
    g_signal_connect_after(G_OBJECT(xkb_groups->btn), "enter", G_CALLBACK(xkb_groups_button_enter), (gpointer) xkb_groups);
    g_signal_connect(G_OBJECT(fbev), "active_window", G_CALLBACK(xkb_groups_active_window_event), xkb_groups);

    /* Show the widget and return. */
    xkb_groups_update(xkb_groups);
    gtk_widget_show(pwid);
    return 1;
}

/* Plugin destructor. */
static void xkb_groups_destructor(Plugin * plugin)
{
    xkb_groups_t * xkb_groups = PRIV(plugin);

    /* Disconnect root window event handler. */
    g_signal_handlers_disconnect_by_func(G_OBJECT(fbev), xkb_groups_active_window_event, xkb_groups);

    /* Disconnect from the XKB mechanism. */
    g_source_remove(xkb_groups->source_id);
    xkb_groups_mechanism_destructor(xkb_groups);

    /* Ensure that the configuration dialog is dismissed. */
    if (xkb_groups->config_dlg != NULL)
        gtk_widget_destroy(xkb_groups->config_dlg);

    /* Deallocate all memory. */
    g_free(xkb_groups);
}

/* Handler for "changed" event on default language combo box of configuration dialog. */
static void xkb_groups_display_type_changed(GtkComboBox * cb, gpointer * data) 
{
    /* Fetch the new value and redraw. */
    xkb_groups_t * xkb_groups = (xkb_groups_t *) data;
    xkb_groups->display_as_text = gtk_combo_box_get_active(cb);
    xkb_groups_update(xkb_groups);
}

/* Handler for "toggled" event on per-application check box of configuration dialog. */
static void xkb_groups_enable_per_application_changed(GtkToggleButton * tb, gpointer * data) 
{
    /* Fetch the new value and redraw. */
    xkb_groups_t * xkb_groups = (xkb_groups_t *) data;
    xkb_groups->per_window_layout = gtk_toggle_button_get_active(tb);
    gtk_widget_set_sensitive(xkb_groups->per_app_default_layout_menu, xkb_groups->per_window_layout);
    xkb_groups_update(xkb_groups);
}

/* Handler for "changed" event on default language combo box of configuration dialog. */
static void xkb_groups_default_language_changed(GtkComboBox * cb, gpointer * data)
{
    /* Fetch the new value and redraw. */
    xkb_groups_t * xkb_groups = (xkb_groups_t *) data;
    xkb_groups->default_group = gtk_combo_box_get_active(cb);
    xkb_groups_update(xkb_groups);
}

/* Handler for "response" event on configuration dialog. */
static void xkb_groups_configuration_response(GtkDialog * dialog, int response, gpointer data)
{
    xkb_groups_t * xkb_groups = (xkb_groups_t *) data;

    /* Save the new configuration and redraw the plugin. */
    plugin_save_configuration(xkb_groups->plugin);
    xkb_groups_update(xkb_groups);

    /* Destroy the dialog. */
    gtk_widget_destroy(xkb_groups->config_dlg);
    xkb_groups->config_dlg = NULL;
}

/* Callback when the configuration dialog is to be shown. */
static void xkb_groups_configure(Plugin * p, GtkWindow * parent)
{
    if (wtl_is_in_kiosk_mode())
        return;

    xkb_groups_t * xkb_groups = PRIV(p);

    /* Create dialog window. */
    GtkWidget * dlg = gtk_dialog_new_with_buttons(
        _("Configure Keyboard Layout Switcher"), 
        NULL,
        GTK_DIALOG_NO_SEPARATOR,
        GTK_STOCK_CLOSE, 
        GTK_RESPONSE_OK,
        NULL);
    if (!dlg)
        return;
    xkb_groups->config_dlg = dlg;
    panel_apply_icon(GTK_WINDOW(dlg));

    /* Create a vertical box as the child of the dialog. */
    GtkWidget * vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox), vbox);

    /* Create a frame as the child of the vertical box. */
    GtkWidget * display_type_frame = gtk_frame_new(NULL);
    gtk_frame_set_label(GTK_FRAME(display_type_frame), _("Show layout as"));
    gtk_box_pack_start(GTK_BOX(vbox), display_type_frame, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(display_type_frame), 5);

    /* Create an alignment as the child of the frame. */
    GtkWidget * alignment2 = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(display_type_frame), alignment2);
    gtk_alignment_set_padding(GTK_ALIGNMENT(alignment2), 4, 4, 10, 10);
  
    /* Create a horizontal box as the child of the alignment. */
    GtkWidget * hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment2), hbox);

    /* Create a combo box as the child of the horizontal box. */

    GtkWidget * display_type_optmenu = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(display_type_optmenu), _("image"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(display_type_optmenu), _("text"));

    gtk_box_pack_start(GTK_BOX(hbox), display_type_optmenu, TRUE, TRUE, 2);
    g_signal_connect(display_type_optmenu, "changed", G_CALLBACK(xkb_groups_display_type_changed), xkb_groups);
    gtk_combo_box_set_active(GTK_COMBO_BOX(display_type_optmenu), xkb_groups->display_as_text);

    /* Create a frame as the child of the vertical box. */
    GtkWidget * per_app_frame = gtk_frame_new(NULL);
    gtk_frame_set_label(GTK_FRAME(per_app_frame), _("Per application settings"));
    gtk_widget_show(per_app_frame);
    gtk_box_pack_start(GTK_BOX(vbox), per_app_frame, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(per_app_frame), 5);

    /* Create an alignment as the child of the frame. */
    GtkWidget * alignment1 = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(per_app_frame), alignment1);
    gtk_alignment_set_padding(GTK_ALIGNMENT(alignment1), 4, 4, 10, 10);

    /* Create a vertical box as the child of the alignment. */
    GtkWidget * per_app_vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment1), per_app_vbox);

    /* Create a check button as the child of the vertical box. */
    GtkWidget * per_app_checkbutton = gtk_check_button_new_with_mnemonic(_("_Remember layout for each application"));
    gtk_box_pack_start(GTK_BOX(per_app_vbox), per_app_checkbutton, FALSE, FALSE, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(per_app_checkbutton), xkb_groups->per_window_layout);
    g_signal_connect(per_app_checkbutton, "toggled", G_CALLBACK(xkb_groups_enable_per_application_changed), xkb_groups);

    /* Create a horizontal box as the child of the vertical box. */
    GtkWidget * hbox3 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(per_app_vbox), hbox3, TRUE, TRUE, 2);

    /* Create a label as the child of the horizontal box. */
    GtkWidget * label4 = gtk_label_new(_("Default layout:"));
    gtk_box_pack_start(GTK_BOX(hbox3), label4, FALSE, FALSE, 2);

    /* Create a combo box as the child of the horizontal box. */
    xkb_groups->per_app_default_layout_menu = gtk_combo_box_text_new();
    gtk_box_pack_start(GTK_BOX(hbox3), xkb_groups->per_app_default_layout_menu, FALSE, TRUE, 2);
    gtk_widget_set_sensitive(xkb_groups->per_app_default_layout_menu, xkb_groups->per_window_layout);

    /* Populate the combo box with the available choices. */
    int i;
    for (i = 0; i < xkb_groups_get_group_count(xkb_groups); i++) 
    {
        gtk_combo_box_text_append_text(
            GTK_COMBO_BOX_TEXT(xkb_groups->per_app_default_layout_menu), 
            xkb_groups_get_symbol_name_by_res_no(xkb_groups, i));
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(xkb_groups->per_app_default_layout_menu), xkb_groups->default_group);
    g_signal_connect(xkb_groups->per_app_default_layout_menu, "changed", G_CALLBACK(xkb_groups_default_language_changed), xkb_groups);

    /* Connect signals. */
    g_signal_connect(xkb_groups->config_dlg, "response", G_CALLBACK(xkb_groups_configuration_response), xkb_groups);

    /* Display the dialog. */
    gtk_widget_set_size_request(GTK_WIDGET(xkb_groups->config_dlg), 400, -1);	/* Improve geometry */
    gtk_widget_show_all(xkb_groups->config_dlg);
    gtk_window_present(GTK_WINDOW(xkb_groups->config_dlg));
}

/* Callback when the configuration is to be saved. */
static void xkb_groups_save_configuration(Plugin * p)
{
    xkb_groups_t * xkb_groups = PRIV(p);
    su_json_write_options(plugin_inner_json(p), option_definitions, xkb_groups);
}

/* Callback when panel configuration changes. */
static void xkb_groups_panel_configuration_changed(Plugin * p)
{
    /* Do a full redraw. */
    xkb_groups_t * xkb_groups = PRIV(p);
    xkb_groups_update(xkb_groups);
}

/* Plugin descriptor. */
PluginClass xkb_groups_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "xkb_groups",
    name : N_("Keyboard Layout Switcher"),
    version: "1.0",
    description : N_("Allows to switch between available keyboard layouts"),
    category: PLUGIN_CATEGORY_SW_INDICATOR,

    constructor : xkb_groups_constructor,
    destructor  : xkb_groups_destructor,
    config : xkb_groups_configure,
    save : xkb_groups_save_configuration,
    panel_configuration_changed : xkb_groups_panel_configuration_changed

};
