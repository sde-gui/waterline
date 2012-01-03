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

#ifndef MISC_H
#define MISC_H

#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdio.h>

#include "panel.h"
#include "plugin.h"

#include "configparser.h"

enum {
    CONF_TYPE_STR,
    CONF_TYPE_INT,
    CONF_TYPE_BOOL,
    CONF_TYPE_FILE,
    CONF_TYPE_FILE_ENTRY,
    CONF_TYPE_DIRECTORY_ENTRY,
    CONF_TYPE_TRIM,
    CONF_TYPE_ENUM,
    CONF_TYPE_TITLE,
    CONF_TYPE_BEGIN_TABLE,
    CONF_TYPE_END_TABLE,
    CONF_TYPE_BEGIN_PAGE,
    CONF_TYPE_END_PAGE,
    CONF_TYPE_COLOR,
    CONF_TYPE_SET_PROPERTY
};

gchar *expand_tilda(gchar *file);

GtkWidget *_gtk_image_new_from_file_scaled(const gchar *file, gint width,
                                           gint height, gboolean keep_ratio, gboolean use_dummy_image);
void get_button_spacing(GtkRequisition *req, GtkContainer *parent, gchar *name);
guint32 gcolor2rgb24(GdkColor *color);
GtkWidget * fb_button_new_from_file(
    gchar * image_file, int width, int height, gulong highlight_color, gboolean keep_ratio);
GtkWidget * fb_button_new_from_file_with_label(
    gchar * image_file, int width, int height, gulong highlight_color, gboolean keep_ratio, Panel * panel, gchar * label);

char* translate_exec_to_cmd( const char* exec, const char* icon,
                             const char* title, const char* fpath );

/*
 This function is used to re-create a new box with different
 orientation from the old one, add all children of the old one to
 the new one, and then destroy the old box.
 It's mainly used when we need to change the orientation of the panel or
 any plugin with a layout box. Since GtkHBox cannot be changed to GtkVBox,
 recreating a new box to replace the old one is required.
*/
GtkWidget* recreate_box( GtkBox* oldbox, GtkOrientation orientation );

void show_error( GtkWindow* parent_win, const char* msg );

/* Parameters: const char* name, gpointer ret_value, GType type, ....NULL */
GtkWidget* create_generic_config_dlg( const char* title, GtkWidget* parent,
                              GSourceFunc apply_func, Plugin * plugin,
                      const char* name, ... );


char* get_config_file( const char* profile, const char* file_name, gboolean is_global );

extern GtkMenu* lxpanel_get_panel_menu( Panel* panel, Plugin* plugin, gboolean use_sub_menu );
extern void     lxpanel_show_panel_menu( Panel* panel, Plugin* plugin, GdkEventButton * event );

extern GdkPixbuf* lxpanel_load_icon( const char* name, int width, int height, gboolean use_fallback );

void fb_button_set_from_file(GtkWidget* btn, const char* img_file, gint width, gint height, gboolean keep_ratio);

gboolean lxpanel_launch_app(const char* exec, GList* files, gboolean in_terminal);
gboolean lxpanel_launch(const char* exec, GList* files);


typedef void (*EntryDialogCallback)(char * value, gpointer payload);

GtkWidget* create_entry_dialog(const char * title, const char * description, const char * value, EntryDialogCallback callback, gpointer payload);

gchar * panel_translate_directory_name(const gchar * name);

int strempty(const char* s);

#endif
