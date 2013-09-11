/**
 * Copyright (c) 2013 Vadim Ushakov
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

#ifndef __WATERLINE__MISC_H
#define __WATERLINE__MISC_H

#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdio.h>
#include <sys/stat.h>

#include <sde-utils-gtk.h>

#include "panel.h"
#include "plugin.h"

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
    CONF_TYPE_RGBA,
    CONF_TYPE_SET_PROPERTY
};

GtkWidget *_gtk_image_new_from_file_scaled(const gchar *file, gint width,
                                           gint height, gboolean keep_ratio, gboolean use_dummy_image);
guint32 gcolor2rgb24(const GdkColor *color);

char* translate_exec_to_cmd( const char* exec, const char* icon,
                             const char* title, const char* fpath );

void show_error( GtkWindow* parent_win, const char* msg );

/* Parameters: const char* name, gpointer ret_value, GType type, ....NULL */
GtkWidget* create_generic_config_dlg( const char* title, GtkWidget* parent,
                              GSourceFunc apply_func, Plugin * plugin,
                      const char* name, ... );


extern GdkPixbuf* wtl_load_icon(const char* name, int width, int height, gboolean use_fallback);
extern GdkPixbuf* wtl_load_icon2(const char* name, int width, int height, gboolean use_fallback, gboolean * themed);

extern void load_window_action_icon(GtkImage * image, const char * name, GtkIconSize icon_size);

gboolean wtl_launch_app(const char* exec, GList* files, gboolean in_terminal);
gboolean wtl_launch(const char* exec, GList* files);
void wtl_open_in_file_manager(const char * path);
void wtl_open_in_terminal(const char * path);
void wtl_open_web_link(const char * link);

typedef void (*EntryDialogCallback)(char * value, gpointer payload);

GtkWidget* create_entry_dialog(const char * title, const char * description, const char * value, EntryDialogCallback callback, gpointer payload);

gchar * panel_translate_directory_name(const gchar * name);

void bring_to_current_desktop(GtkWidget * win);

gchar * wtl_tooltip_for_file_stat(struct stat * stat_data);


extern void filemodestring (struct stat const *statp, char *str);

/* Get the declaration of strmode.  */
# if HAVE_DECL_STRMODE
#  include <string.h> /* MacOS X, FreeBSD, OpenBSD */
#  include <unistd.h> /* NetBSD */
# endif

# if !HAVE_DECL_STRMODE
extern void strmode (mode_t mode, char *str);
# endif

void restore_grabs(GtkWidget *w, gpointer data);

gboolean is_my_own_window(Window window);

void color_parse_d(const char * src, double dst[3]);

gchar ** read_list_from_config(gchar * file_name);

const char * get_de_name(void);


static inline void cairo_set_source_gdkrgba(cairo_t * cr, GdkRGBA * rgba)
{
    cairo_set_source_rgba(cr, rgba->red, rgba->green, rgba->blue, rgba->alpha);
}

static inline void mix_rgba(GdkRGBA * result, GdkRGBA * a, GdkRGBA * b, double v)
{
    result->red   = a->red   * v + b->red   * (1.0 - v);
    result->green = a->green * v + b->green * (1.0 - v);
    result->blue  = a->blue  * v + b->blue  * (1.0 - v);
    result->alpha = a->alpha * v + b->alpha * (1.0 - v);
}

void rgba_to_color(GdkRGBA * rgba, GdkColor * color, guint16 * alpha);
void color_to_rgba(GdkRGBA * rgba, GdkColor * color, guint16 * alpha);

#endif
