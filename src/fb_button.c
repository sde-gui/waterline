/**
 * Copyright (c) 2011-2013 Vadim Ushakov
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

#include <lxpanelx/fb_button.h>
#include <lxpanelx/global.h>
#include <lxpanelx/misc.h>
#include <lxpanelx/panel.h>
#include "panel_internal.h"
#include <lxpanelx/Xsupport.h>
#include <lxpanelx/dbg.h>

#include <lxpanelx/gtkcompat.h>

/********************************************************************/

GdkPixbuf * _gdk_pixbuf_new_from_file_at_scale(const char * file_name, int width, int height, gboolean keep_ratio);

/********************************************************************/

/* data used by themed images buttons */
typedef struct {
    char* fname;
    guint theme_changed_handler;
    GdkPixbuf* pixbuf;
    GdkPixbuf* pixbuf_highlighted;
    gulong highlight_color;
    int dw, dh; /* desired size */
    gboolean use_dummy_image;
} ImgData;

static GQuark img_data_id = 0;

static void on_theme_changed(GtkIconTheme* theme, GtkWidget* img);
static void _gtk_image_set_from_file_scaled( GtkWidget* img, const gchar *file, gint width, gint height, gboolean keep_ratio, gboolean use_dummy_image);

/* DestroyNotify handler for image data in _gtk_image_new_from_file_scaled. */
static void img_data_free(ImgData * data)
{
    g_free(data->fname);
    if (data->theme_changed_handler != 0)
        g_signal_handler_disconnect(gtk_icon_theme_get_default(), data->theme_changed_handler);
    if (data->pixbuf != NULL)
        g_object_unref(data->pixbuf);
    if (data->pixbuf_highlighted != NULL)
        g_object_unref(data->pixbuf_highlighted);
    g_free(data);
}

/* Handler for "changed" signal in _gtk_image_new_from_file_scaled. */
static void on_theme_changed(GtkIconTheme * theme, GtkWidget * img)
{
    ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(img), img_data_id);
    _gtk_image_set_from_file_scaled(img, data->fname, data->dw, data->dh, TRUE, data->use_dummy_image);
}

void fb_button_set_orientation(GtkWidget * btn, GtkOrientation orientation)
{
    GtkWidget * child = gtk_bin_get_child(GTK_BIN(btn));
    if (GTK_IS_BOX(child))
    {
        GtkBox *  newbox = GTK_BOX(recreate_box(GTK_BOX(child), orientation));
        if (GTK_WIDGET(newbox) != child)
        {
            gtk_container_add(GTK_CONTAINER(btn), GTK_WIDGET(newbox));
        }
    }
}

void fb_button_set_label(GtkWidget * btn, Plugin * plugin, gchar * label)
{
    /* Locate the label within the button. */
    GtkWidget * child = gtk_bin_get_child(GTK_BIN(btn));
    GtkWidget * lbl = NULL;
    if (GTK_IS_IMAGE(child))
    {
        /* No label. Create new. */

        GtkWidget * img = child;

        GtkWidget * inner = gtk_hbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 0);
        GTK_WIDGET_UNSET_FLAGS (inner, GTK_CAN_FOCUS);

        g_object_ref(G_OBJECT(img));
        gtk_container_remove(GTK_CONTAINER(btn), img);
        gtk_container_add(GTK_CONTAINER(btn), inner);

        gtk_box_pack_start(GTK_BOX(inner), img, FALSE, FALSE, 0);

        g_object_unref(G_OBJECT(img));

        lbl = gtk_label_new("");
        gtk_misc_set_padding(GTK_MISC(lbl), 2, 0);
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);

        gtk_widget_show_all(inner);
    }
    else if (GTK_IS_BOX(child))
    {
        GList * children = gtk_container_get_children(GTK_CONTAINER(child));
        if (children->next)
            lbl = GTK_WIDGET(GTK_IMAGE(children->next->data));
        g_list_free(children);
    }


    /* Update label text. */
    if (lbl)
        panel_draw_label_text(plugin_panel(plugin), lbl, label, FALSE, FALSE, FALSE, TRUE);
}

void fb_button_set_from_file(GtkWidget * btn, const char * img_file, gint width, gint height)
{
    /* Locate the image within the button. */
    GtkWidget * child = gtk_bin_get_child(GTK_BIN(btn));
    GtkWidget * image = NULL;
    GtkWidget * label = NULL;

    if (GTK_IS_IMAGE(child))
    {
        image = child;
    }
    else if (GTK_IS_BOX(child))
    {
        GList * children = gtk_container_get_children(GTK_CONTAINER(child));
        image = GTK_WIDGET(GTK_IMAGE(children->data));
        if (children->next)
            label = GTK_WIDGET(GTK_IMAGE(children->next->data));
        g_list_free(children);
    }

    if (image)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(image), img_data_id);
        g_free(data->fname);
        data->fname = g_strdup(img_file);
        data->dw = width;
        data->dh = height;
        data->use_dummy_image = label && !strempty(gtk_label_get_text(GTK_LABEL(label)));
        _gtk_image_set_from_file_scaled(image,
            data->fname, data->dw, data->dh, TRUE, data->use_dummy_image);

        gtk_widget_set_visible(image, !(strempty(img_file) && data->use_dummy_image));
    }
}

static void _gtk_image_set_from_file_scaled(GtkWidget * img, const gchar * file, gint width, gint height, gboolean keep_ratio, gboolean use_dummy_image)
{
    ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(img), img_data_id);
    data->dw = width;
    data->dh = height;
    data->use_dummy_image = use_dummy_image;

    if (data->pixbuf != NULL)
    {
        g_object_unref(data->pixbuf);
        data->pixbuf = NULL;
    }

    /* if there is a cached hilighted version of this pixbuf, free it */
    if (data->pixbuf_highlighted != NULL)
    {
        g_object_unref(data->pixbuf_highlighted);
        data->pixbuf_highlighted = NULL;
    }

    /* if they are the same string, eliminate unnecessary copy. */
    gboolean themed = FALSE;
    if (file != NULL && strlen(file) != 0)
    {
        if (data->fname != file)
        {
            g_free(data->fname);
            data->fname = g_strdup(file);
        }

        if (g_file_test(file, G_FILE_TEST_EXISTS))
        {
            GdkPixbuf * pb_scaled = _gdk_pixbuf_new_from_file_at_scale(file, width, height, keep_ratio);
            if (pb_scaled != NULL)
                data->pixbuf = pb_scaled;
        }
        else
        {
            data->pixbuf = lxpanel_load_icon(file, width, height, keep_ratio);
            themed = TRUE;
        }
    }

    if (data->pixbuf != NULL)
    {
        /* Set the pixbuf into the image widget. */
        gtk_image_set_from_pixbuf((GtkImage *)img, data->pixbuf);
        if (themed)
        {
            /* This image is loaded from icon theme.  Update the image if the icon theme is changed. */
            if (data->theme_changed_handler == 0)
                data->theme_changed_handler = g_signal_connect(gtk_icon_theme_get_default(), "changed", G_CALLBACK(on_theme_changed), img);
        }
        else
        {
            /* This is not loaded from icon theme.  Disconnect the signal handler. */
            if (data->theme_changed_handler != 0)
            {
                g_signal_handler_disconnect(gtk_icon_theme_get_default(), data->theme_changed_handler);
                data->theme_changed_handler = 0;
            }
        }
    }
    else
    {
        /* No pixbuf available.  Set the "missing image" icon. */
        if (data->use_dummy_image)
            gtk_image_set_from_stock(GTK_IMAGE(img), GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_BUTTON);
    }
    return;
}

GtkWidget * _gtk_image_new_from_file_scaled(const gchar * file, gint width, gint height, gboolean keep_ratio, gboolean use_dummy_image)
{
    GtkWidget * img = gtk_image_new();
    ImgData * data = g_new0(ImgData, 1);
    if (img_data_id == 0)
        img_data_id = g_quark_from_static_string("ImgData");
    g_object_set_qdata_full(G_OBJECT(img), img_data_id, data, (GDestroyNotify) img_data_free);
    _gtk_image_set_from_file_scaled(img, file, width, height, keep_ratio, use_dummy_image);

    gtk_misc_set_alignment(GTK_MISC(img), 0.5, 0.5);
    gtk_misc_set_padding (GTK_MISC(img), 0, 0);

    return img;
}


/* Handler for "enter-notify-event" signal on image that has highlighting requested. */
static gboolean fb_button_enter(GtkImage * widget, GdkEventCrossing * event)
{
    if (gtk_image_get_storage_type(widget) == GTK_IMAGE_PIXBUF)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(widget), img_data_id);
        if (data != NULL)
        {
            if (data->pixbuf_highlighted == NULL)
            {
                GdkPixbuf * dark = data->pixbuf;
                int height = gdk_pixbuf_get_height(dark);
                int rowstride = gdk_pixbuf_get_rowstride(dark);
                gulong highlight_color = data->highlight_color;

                GdkPixbuf * light = gdk_pixbuf_add_alpha(dark, FALSE, 0, 0, 0);
                if (light != NULL)
                {
                    guchar extra[3];
                    int i;
                    for (i = 2; i >= 0; i--, highlight_color >>= 8)
                        extra[i] = highlight_color & 0xFF;

                    guchar * src = gdk_pixbuf_get_pixels(light);
                    guchar * up;
                    for (up = src + height * rowstride; src < up; src += 4)
                    {
                        if (src[3] != 0)
                        {
                            for (i = 0; i < 3; i++)
                            {
                            int value = src[i] + extra[i];
                            if (value > 255) value = 255;
                            src[i] = value;
                            }
                        }
                    }
                    data->pixbuf_highlighted = light;
                }
            }

        if (data->pixbuf_highlighted != NULL)
            gtk_image_set_from_pixbuf(widget, data->pixbuf_highlighted);
        }
    }
    return TRUE;
}

/* Handler for "leave-notify-event" signal on image that has highlighting requested. */
static gboolean fb_button_leave(GtkImage * widget, GdkEventCrossing * event, gpointer user_data)
{
    if (gtk_image_get_storage_type(widget) == GTK_IMAGE_PIXBUF)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(widget), img_data_id);
        if ((data != NULL) && (data->pixbuf != NULL))
            gtk_image_set_from_pixbuf(widget, data->pixbuf);
    }
    return TRUE;
}


GtkWidget * fb_button_new_from_file(gchar * image_file, int width, int height, Plugin * plugin)
{
    return fb_button_new_from_file_with_label(image_file, width, height, plugin, NULL);
}

GtkWidget * fb_button_new_from_file_with_label(gchar * image_file, int width, int height, Plugin * plugin, gchar * label)
{
    gulong highlight_color = (TRUE /* TODO: read setting from plugin object */) ? PANEL_ICON_HIGHLIGHT : 0;

    GtkWidget * event_box = gtk_event_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(event_box), 0);
    gtk_widget_set_has_window(event_box, FALSE);
    GTK_WIDGET_UNSET_FLAGS(event_box, GTK_CAN_FOCUS);

    GtkWidget * image = _gtk_image_new_from_file_scaled(image_file, width, height, TRUE, !label || strlen(label) == 0);
    gtk_misc_set_padding(GTK_MISC(image), 0, 0);
    gtk_misc_set_alignment(GTK_MISC(image), 0, 0);
    if (highlight_color != 0)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(image), img_data_id);
        data->highlight_color = highlight_color;

        gtk_widget_add_events(event_box, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
        g_signal_connect_swapped(G_OBJECT(event_box), "enter-notify-event", G_CALLBACK(fb_button_enter), image);
        g_signal_connect_swapped(G_OBJECT(event_box), "leave-notify-event", G_CALLBACK(fb_button_leave), image);
    }

    if (label == NULL)
        gtk_container_add(GTK_CONTAINER(event_box), image);
    else
    {
        GtkWidget * inner = gtk_hbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 0);
        GTK_WIDGET_UNSET_FLAGS (inner, GTK_CAN_FOCUS);
        gtk_container_add(GTK_CONTAINER(event_box), inner);

        gtk_box_pack_start(GTK_BOX(inner), image, FALSE, FALSE, 0);

        GtkWidget * lbl = gtk_label_new("");
        panel_draw_label_text(plugin_panel(plugin), lbl, label, FALSE, FALSE, FALSE, TRUE);
        gtk_misc_set_padding(GTK_MISC(lbl), 2, 0);
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);
    }

    gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.5);
    gtk_misc_set_padding (GTK_MISC(image), 0, 0);

    gtk_widget_show_all(event_box);

    if (plugin)
        panel_require_update_background(plugin_panel(plugin));

    return event_box;
}

