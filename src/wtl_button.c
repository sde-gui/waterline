/**
 * Copyright (c) 2015 Vadim Ushakov
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

#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <pwd.h>
#include <grp.h>

#include <sde-utils.h>

#include <waterline/global.h>
#include <waterline/misc.h>
#include <waterline/panel.h>
#include "panel_internal.h"
#include <waterline/x11_utils.h>
#include <waterline/gtkcompat.h>

/********************************************************************/

static GQuark data_pointer_id = 0;

typedef struct {
    Plugin * plugin;
    int image_size;
    gchar * image_name;
    gchar * label_text;
    gboolean use_markup;
    GtkWidget * event_box;
    GtkWidget * inner_box;
    GtkWidget * label;
    GtkWidget * image;

    GdkPixbuf * pixbuf;
    GdkPixbuf * pixbuf_fallback;
    GdkPixbuf * pixbuf_highlighted;

    gboolean mouse_over;

    guint theme_changed_handler;
} WtlButtonData;

/********************************************************************/

static void on_theme_changed(GtkIconTheme* theme, WtlButtonData * button_data);

/********************************************************************/

static void wtl_button_data_free_pixbufs(WtlButtonData * button_data)
{
    if (button_data->pixbuf)
    {
        g_object_unref(button_data->pixbuf);
        button_data->pixbuf = NULL;
    }

    if (button_data->pixbuf_fallback)
    {
        g_object_unref(button_data->pixbuf_fallback);
        button_data->pixbuf_fallback = NULL;
    }

    if (button_data->pixbuf_highlighted)
    {
        g_object_unref(button_data->pixbuf_highlighted);
        button_data->pixbuf_highlighted = NULL;
    }
}

static void wtl_button_data_free(WtlButtonData * button_data)
{
    wtl_button_data_free_pixbufs(button_data);

    if (button_data->theme_changed_handler != 0)
        g_signal_handler_disconnect(gtk_icon_theme_get_default(), button_data->theme_changed_handler);

    g_free(button_data->image_name);
    g_free(button_data->label_text);
    g_free(button_data);
}

/********************************************************************/

static GdkPixbuf * draw_highlighted_pixbuf(GdkPixbuf * source_pixbuf, gulong highlight_color)
{
    if (!source_pixbuf)
        return NULL;

    int height = gdk_pixbuf_get_height(source_pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(source_pixbuf);

    GdkPixbuf * pixbuf = gdk_pixbuf_add_alpha(source_pixbuf, FALSE, 0, 0, 0);
    if (pixbuf)
    {
        guchar extra[3];
        int i;
        for (i = 2; i >= 0; i--, highlight_color >>= 8)
            extra[i] = highlight_color & 0xFF;

        guchar * src = gdk_pixbuf_get_pixels(pixbuf);
        guchar * up;
        for (up = src + height * rowstride; src < up; src += 4)
        {
            if (src[3] != 0)
            {
                for (i = 0; i < 3; i++)
                {
                    int value = src[i] + extra[i];
                    if (value > 255)
                        value = 255;
                    src[i] = value;
                }
            }
        }
    }

    return pixbuf;
}

/********************************************************************/

static void apply_pixbuf(WtlButtonData * button_data)
{
    if (!button_data->image)
        return;

    if (button_data->mouse_over)
    {
        gulong highlight_color = (TRUE /* TODO: read setting from plugin object */) ? PANEL_ICON_HIGHLIGHT : 0;
        if (!button_data->pixbuf_highlighted)
            button_data->pixbuf_highlighted = draw_highlighted_pixbuf(
                button_data->pixbuf ? button_data->pixbuf : button_data->pixbuf_fallback, highlight_color);
        if (button_data->pixbuf_highlighted)
            gtk_image_set_from_pixbuf(GTK_IMAGE(button_data->image), button_data->pixbuf_highlighted);
    }
    else
    {
        gtk_image_set_from_pixbuf(GTK_IMAGE(button_data->image),
            button_data->pixbuf ? button_data->pixbuf : button_data->pixbuf_fallback);
    }
}

static void apply_image_and_label(WtlButtonData * button_data)
{
    if (su_str_empty_nl(button_data->label_text))
    {
        if (button_data->label)
            gtk_widget_hide(button_data->label);
    }
    else
    {
        if (!button_data->label)
        {
            button_data->label = gtk_label_new(NULL);
            gtk_misc_set_padding(GTK_MISC(button_data->label), 2, 0);
            gtk_box_pack_end(GTK_BOX(button_data->inner_box), button_data->label, FALSE, FALSE, 0);
        }
        panel_draw_label_text(plugin_panel(button_data->plugin),
            button_data->label,
            button_data->label_text,
            STYLE_CUSTOM_COLOR | (button_data->use_markup ? STYLE_MARKUP : 0));
        gtk_widget_show(button_data->label);
    }

    if (su_str_empty_nl(button_data->image_name) && !su_str_empty_nl(button_data->label_text))
    {
        if (button_data->image)
            gtk_widget_hide(button_data->image);
        wtl_button_data_free_pixbufs(button_data);
    }
    else
    {
        if (!button_data->pixbuf)
        {
            button_data->pixbuf = wtl_load_icon(button_data->image_name,
                button_data->image_size, button_data->image_size, FALSE);
        }

        if (!button_data->pixbuf_fallback)
        {
            button_data->pixbuf_fallback = wtl_load_icon(button_data->image_name,
                button_data->image_size, button_data->image_size, TRUE);
        }

        if (!button_data->image)
        {
            button_data->image = gtk_image_new();
            gtk_misc_set_padding(GTK_MISC(button_data->image), 0, 0);
            gtk_misc_set_alignment(GTK_MISC(button_data->image), 0.5, 0.5);
            gtk_box_pack_start(GTK_BOX(button_data->inner_box), button_data->image, FALSE, FALSE, 0);
        }

        apply_pixbuf(button_data);

        if (button_data->pixbuf || su_str_empty_nl(button_data->label_text))
            gtk_widget_show(button_data->image);
        else
            gtk_widget_hide(button_data->image);


        if (button_data->theme_changed_handler == 0)
            button_data->theme_changed_handler = g_signal_connect(
                gtk_icon_theme_get_default(), "changed", G_CALLBACK(on_theme_changed), button_data);
    }
}

/********************************************************************/

static void on_theme_changed(GtkIconTheme * theme, WtlButtonData * button_data)
{
    wtl_button_data_free_pixbufs(button_data);
    apply_image_and_label(button_data);
}

/********************************************************************/

static gboolean on_button_enter(GtkWidget * widget, GdkEventCrossing * event, WtlButtonData * button_data)
{
    button_data->mouse_over = TRUE;
    apply_pixbuf(button_data);
    return TRUE;
}

static gboolean on_button_leave(GtkWidget * widget, GdkEventCrossing * event, WtlButtonData * button_data)
{
    button_data->mouse_over = FALSE;
    apply_pixbuf(button_data);
    return TRUE;
}

/********************************************************************/

static GtkWidget * wtl_button_new_from_image_name_with_label(Plugin * plugin, gchar * image_name, int size, const char * label_text, gboolean use_markup)
{
    GtkWidget * event_box = gtk_event_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(event_box), 0);
    gtk_widget_set_has_window(event_box, FALSE);
    GTK_WIDGET_UNSET_FLAGS(event_box, GTK_CAN_FOCUS);

    if (data_pointer_id == 0)
        data_pointer_id = g_quark_from_static_string("wtl_button_data_pointer");

    WtlButtonData * button_data = g_new0(WtlButtonData, 1);
    g_object_set_qdata_full(G_OBJECT(event_box), data_pointer_id, button_data, (GDestroyNotify) wtl_button_data_free);

    button_data->plugin = plugin;
    button_data->image_size = size;
    button_data->image_name = g_strdup(image_name);
    button_data->label_text = g_strdup(label_text);
    button_data->use_markup = use_markup;
    button_data->event_box = event_box;

    gtk_widget_add_events(event_box, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(G_OBJECT(event_box), "enter-notify-event", G_CALLBACK(on_button_enter), button_data);
    g_signal_connect(G_OBJECT(event_box), "leave-notify-event", G_CALLBACK(on_button_leave), button_data);

    button_data->inner_box = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(button_data->inner_box), 0);
    GTK_WIDGET_UNSET_FLAGS(button_data->inner_box, GTK_CAN_FOCUS);
    gtk_container_add(GTK_CONTAINER(event_box), button_data->inner_box);
    gtk_widget_show(button_data->inner_box);

    apply_image_and_label(button_data);

    return event_box;
}

GtkWidget * wtl_button_new(Plugin * plugin)
{
    return wtl_button_new_from_image_name_with_label(plugin, NULL, 0, NULL, FALSE);
}

GtkWidget * wtl_button_new_from_image_name(Plugin * plugin, gchar * image_name, int size)
{
    return wtl_button_new_from_image_name_with_label(plugin, image_name, size, NULL, FALSE);
}

GtkWidget * wtl_button_new_from_image_name_with_text(Plugin * plugin, gchar * image_name, int size, const char * text)
{
    return wtl_button_new_from_image_name_with_label(plugin, image_name, size, text, FALSE);
}

GtkWidget * wtl_button_new_from_image_name_with_markup(Plugin * plugin, gchar * image_name, int size, const char * markup)
{
    return wtl_button_new_from_image_name_with_label(plugin, image_name, size, markup, TRUE);
}

void wtl_button_set_orientation(GtkWidget * button, GtkOrientation orientation)
{
    WtlButtonData * button_data = (WtlButtonData *) g_object_get_qdata(G_OBJECT(button), data_pointer_id);
    if (!button_data)
        return;

    gtk_orientable_set_orientation(GTK_ORIENTABLE(button_data->inner_box), orientation);
}

void wtl_button_set_image_name(GtkWidget * button, const char * image_name, int size)
{
    WtlButtonData * button_data = (WtlButtonData *) g_object_get_qdata(G_OBJECT(button), data_pointer_id);
    if (!button_data)
        return;

    if (size >= 0)
        button_data->image_size = size;

    g_free(button_data->image_name);
    button_data->image_name = g_strdup(image_name);
    wtl_button_data_free_pixbufs(button_data);
    apply_image_and_label(button_data);
}

static void wtl_button_set_label(GtkWidget * button, const char * label_text, gboolean use_markup)
{
    WtlButtonData * button_data = (WtlButtonData *) g_object_get_qdata(G_OBJECT(button), data_pointer_id);
    if (!button_data)
        return;

    g_free(button_data->label_text);
    button_data->label_text = g_strdup(label_text);
    button_data->use_markup = use_markup;
    apply_image_and_label(button_data);
}


void wtl_button_set_label_text(GtkWidget * button, const char * text)
{
    wtl_button_set_label(button, text, FALSE);
}

void wtl_button_set_label_markup(GtkWidget * button, const char * markup)
{
    wtl_button_set_label(button, markup, TRUE);
}

