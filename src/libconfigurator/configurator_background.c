/**
 *
 * Copyright (c) 2011-2015 Vadim Ushakov
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
#include "config.h"
#endif

#include <waterline/global.h>
#include <waterline/panel.h>
#include "panel_internal.h"
#include "panel_private.h"
#include <waterline/paths.h>
#include <glib/gi18n.h>

/******************************************************************************/

static void stretch_background_toggle(GtkWidget * w, Panel*  p)
{
    gboolean t = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));

    p->stretch_background = t;
    panel_update_background(p);
}

static void alpha_scale_value_changed(GtkWidget * w, Panel*  p)
{
    int alpha = gtk_range_get_value(GTK_RANGE(w));

    if (p->alpha != alpha)
    {
        p->alpha = alpha;
        panel_update_background(p);

        GtkWidget* tr = (GtkWidget*)g_object_get_data(G_OBJECT(w), "background_color");
        gtk_color_button_set_alpha(GTK_COLOR_BUTTON(tr), 256 * p->alpha);
    }
}

static void background_color_toggled(GtkWidget * b, Panel * p)
{
    GtkWidget* tr = (GtkWidget*)g_object_get_data(G_OBJECT(b), "background_color");
    gboolean t;

    t = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b));
    gtk_widget_set_sensitive(tr, t);

    p->background_mode = BACKGROUND_COLOR;
    panel_update_background(p);
}

static void background_file_helper(Panel * p, GtkWidget * toggle, GtkFileChooser * file_chooser)
{
    char * file = g_strdup(gtk_file_chooser_get_filename(file_chooser));
    if (file != NULL)
    {
        g_free(p->background_file);
        p->background_file = file;
    }

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)))
    {
        p->background_mode = BACKGROUND_IMAGE;
        panel_update_background(p);
    }
}

static void background_image_toggled(GtkWidget * b, Panel * p)
{
    GtkWidget * fc = (GtkWidget*) g_object_get_data(G_OBJECT(b), "img_file");
    gtk_widget_set_sensitive(fc, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b)));
    background_file_helper(p, b, GTK_FILE_CHOOSER(fc));
}

static void background_changed(GtkFileChooser *file_chooser,  Panel* p )
{
    GtkWidget * btn = GTK_WIDGET(g_object_get_data(G_OBJECT(file_chooser), "bg_image"));
    background_file_helper(p, btn, file_chooser);
}

static void background_system_toggled(GtkWidget *b, Panel* p)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b))) {
        p->background_mode = BACKGROUND_SYSTEM;
        panel_update_background(p);
    }
}

static void on_background_color_set( GtkColorButton* clr,  Panel* p )
{
    gtk_color_button_get_color(clr, &p->background_color);
    p->alpha = gtk_color_button_get_alpha( clr ) / 256;
    panel_update_background( p );

    GtkWidget * alpha_scale = (GtkWidget*)g_object_get_data(G_OBJECT(clr), "alpha_scale");
    gtk_range_set_value(GTK_RANGE(alpha_scale), p->alpha);
}

void initialize_background_controls(Panel * p, GtkBuilder * builder)
{
    GtkWidget *w, *background_color;

    /* transparancy */
    background_color = w = (GtkWidget*)gtk_builder_get_object(builder, "background_color");
    gtk_color_button_set_color((GtkColorButton*)w, &p->background_color);
    gtk_color_button_set_alpha((GtkColorButton*)w, 256 * p->alpha);
    gtk_widget_set_sensitive(w, p->background_mode == BACKGROUND_COLOR);
    g_signal_connect( w, "color-set", G_CALLBACK( on_background_color_set ), p );

    GtkWidget * alpha_scale = (GtkWidget*)gtk_builder_get_object(builder, "alpha_scale");
    gtk_range_set_range(GTK_RANGE(alpha_scale), 0, 255);
    gtk_range_set_value(GTK_RANGE(alpha_scale), p->alpha);
    g_object_set_data(G_OBJECT(alpha_scale), "background_color", background_color);
    g_object_set_data(G_OBJECT(background_color), "alpha_scale", alpha_scale);
    g_signal_connect(alpha_scale, "value-changed", G_CALLBACK(alpha_scale_value_changed), p);

    /* stretch_background */
    GtkWidget * stretch_background = (GtkWidget*)gtk_builder_get_object(builder, "stretch_background");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stretch_background), p->stretch_background);
    g_signal_connect(stretch_background, "toggled", G_CALLBACK(stretch_background_toggle), p);

    w = (GtkWidget*)gtk_builder_get_object(builder, "opacity_info_label");
    gtk_widget_set_visible(w, !panel_is_composited(p));

    /* background */
    {
        GtkWidget * button_bg_system = (GtkWidget*)gtk_builder_get_object( builder, "bg_mode_system" );
        GtkWidget * button_bg_color  = (GtkWidget*)gtk_builder_get_object( builder, "bg_mode_color" );
        GtkWidget * button_bg_image  = (GtkWidget*)gtk_builder_get_object( builder, "bg_mode_image" );

        g_object_set_data(G_OBJECT(button_bg_color), "background_color", background_color);

        if (p->background_mode == BACKGROUND_IMAGE)
            gtk_toggle_button_set_active( (GtkToggleButton*)button_bg_image, TRUE);
        else if (p->background_mode == BACKGROUND_COLOR)
            gtk_toggle_button_set_active( (GtkToggleButton*)button_bg_color, TRUE);
        else
            gtk_toggle_button_set_active( (GtkToggleButton*)button_bg_system, TRUE);

        g_signal_connect(button_bg_system, "toggled", G_CALLBACK(background_system_toggled), p);
        g_signal_connect(button_bg_color, "toggled", G_CALLBACK(background_color_toggled), p);
        g_signal_connect(button_bg_image, "toggled", G_CALLBACK(background_image_toggled), p);

        w = (GtkWidget*)gtk_builder_get_object( builder, "img_file" );
        g_object_set_data(G_OBJECT(button_bg_image), "img_file", w);
        gchar * default_backgroud_path = wtl_resolve_own_resource("", "images", "background.png", 0);
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(w),
            ((p->background_file != NULL) ? p->background_file : default_backgroud_path));
        g_free(default_backgroud_path);

        gtk_widget_set_sensitive(w, p->background_mode == BACKGROUND_IMAGE);
        g_object_set_data( G_OBJECT(w), "bg_image", button_bg_image);
        g_signal_connect( w, "file-set", G_CALLBACK (background_changed), p);
    }

}
