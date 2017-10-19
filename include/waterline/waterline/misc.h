/**
 * Copyright (c) 2013-2015 Vadim Ushakov
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

extern GtkWidget * wtl_gtk_widget_show(GtkWidget * widget);
extern GtkWidget * wtl_gtk_widget_hide(GtkWidget * widget);

extern GtkWidget * wtl_load_icon_as_gtk_image(const char * name, int width, int height);

extern void wtl_show_error_message(GtkWindow * parent_window, const char * message);

/* Parameters: const char* name, gpointer ret_value, GType type, ....NULL */
extern GtkWidget * wtl_create_generic_config_dialog(const char * title, GtkWidget * parent,
    GSourceFunc apply_func, Plugin * plugin,
    const char * name, ...);


typedef void (*EntryDialogCallback)(char * value, gpointer payload);

extern GtkWidget* wtl_create_entry_dialog(const char * title, const char * description, const char * value, EntryDialogCallback callback, gpointer payload);


extern GdkPixbuf* wtl_load_icon(const char* name, int width, int height, gboolean use_fallback);
extern void wtl_load_window_action_icon(GtkImage * image, const char * name, GtkIconSize icon_size);

extern void wtl_util_bring_window_to_current_desktop(GtkWidget * win);

extern gchar * wtl_tooltip_for_file_stat(struct stat * stat_data);

extern void restore_grabs(GtkWidget *w, gpointer data);

extern void color_parse_d(const char * src, double dst[3]);

extern gchar ** read_list_from_config(gchar * file_name);

extern const char * wtl_get_de_name(void);


static inline void cairo_set_source_gdkrgba(cairo_t * cr, GdkRGBA * rgba)
{
    cairo_set_source_rgba(cr, rgba->red, rgba->green, rgba->blue, rgba->alpha);
}

static inline void mix_rgba(GdkRGBA * result, GdkRGBA * a, GdkRGBA * b, double v)
{
    if (v < 0.0)
        v = 0.0;
    else if (v > 1.0)
        v = 1.0;
    result->red   = a->red   * v + b->red   * (1.0 - v);
    result->green = a->green * v + b->green * (1.0 - v);
    result->blue  = a->blue  * v + b->blue  * (1.0 - v);
    result->alpha = a->alpha * v + b->alpha * (1.0 - v);
}

static inline double rescale_range(double value, double range1_l, double range1_h, double range2_l, double range2_h)
{
    double v = value;
    v -= range1_l;
    if (v < 0.0)
        v = 0.0;
    v /= (range1_h - range1_l);
    if (v > 1.0)
        v = 1.0;
    v *= (range2_h - range2_l);
    v += range2_l;
    return v;
}

/****************************************************************************/

extern guint32 wtl_util_gdkcolor_to_uint32(const GdkColor * color);
extern void wtl_util_gdkrgba_to_gdkcolor(GdkRGBA * rgba, GdkColor * color, guint16 * alpha);
extern void wtl_util_gdkcolor_to_gdkrgba(GdkRGBA * rgba, GdkColor * color, guint16 * alpha);

/****************************************************************************/

extern void wtl_show_run_box(void);

#endif
