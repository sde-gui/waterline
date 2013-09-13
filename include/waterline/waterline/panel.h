/**
 * Copyright (c) 2011-2012 Vadim Ushakov
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

#ifndef __WATERLINE__PANEL_H
#define __WATERLINE__PANEL_H

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "typedef.h"

enum { ALIGN_NONE, ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT  };
enum { EDGE_NONE, EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM };
enum { WIDTH_NONE, WIDTH_REQUEST, WIDTH_PIXEL, WIDTH_PERCENT };
enum { HEIGHT_NONE, HEIGHT_PIXEL, HEIGHT_REQUEST };
enum { VISIBILITY_ALWAYS, VISIBILITY_BELOW, VISIBILITY_AUTOHIDE, VISIBILITY_GOBELOW };
enum {
    ORIENT_NONE = -1,
    ORIENT_VERT = GTK_ORIENTATION_VERTICAL,
    ORIENT_HORIZ = GTK_ORIENTATION_HORIZONTAL
};
enum { POS_NONE, POS_START, POS_END };

enum { OUTPUT_WHOLE_SCREEN, OUTPUT_PRIMARY_MONITOR, OUTPUT_CUSTOM_MONITOR };

#define STYLE_BOLD         (1 << 0)
#define STYLE_ITALIC       (1 << 1)
#define STYLE_UNDERLINE    (1 << 2)
#define STYLE_CUSTOM_COLOR (1 << 3)
#define STYLE_MARKUP       (1 << 4)

extern GtkStyle * panel_get_default_style(Panel * p);

extern GtkWidget * panel_get_toplevel_widget(Panel * p);
extern GdkWindow * panel_get_toplevel_window(Panel * p);
extern Window      panel_get_toplevel_xwindow(Panel * p);
extern GdkColormap * panel_get_color_map(Panel * p);

extern int panel_get_edge(Panel * p);
extern int panel_get_oriented_height_pixels(Panel * p);

extern gboolean panel_is_composited(Panel * p);
extern gboolean panel_is_composite_available(Panel * p);

extern void panel_draw_label_text(Panel * p, GtkWidget * label, const  char * text, unsigned style);
extern void panel_draw_label_text_with_font(Panel * p, GtkWidget * label, const char * text, unsigned style, const char * custom_font_desc);

extern void panel_image_set_from_file(Panel * p, GtkWidget * image, char * file);
extern gboolean panel_image_set_icon_theme(Panel * p, GtkWidget * image, const gchar * icon);

extern GList * panel_get_plugins(Panel * p);

extern void panel_application_class_visibility_changed(Panel* p);
extern gboolean panel_is_application_class_visible(Panel* p, const char * class_name);

#endif
