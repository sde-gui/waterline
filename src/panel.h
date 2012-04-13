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

#ifndef PANEL_H
#define PANEL_H

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "config.h"

#include "bg.h"
#include "ev.h"

enum { ALLIGN_NONE, ALLIGN_LEFT, ALLIGN_CENTER, ALLIGN_RIGHT  };
enum { EDGE_NONE, EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM };
enum { WIDTH_NONE, WIDTH_REQUEST, WIDTH_PIXEL, WIDTH_PERCENT };
enum { HEIGHT_NONE, HEIGHT_PIXEL, HEIGHT_REQUEST };
enum {
    ORIENT_NONE = -1,
    ORIENT_VERT = GTK_ORIENTATION_VERTICAL,
    ORIENT_HORIZ = GTK_ORIENTATION_HORIZONTAL
};
enum { POS_NONE, POS_START, POS_END };

#define PANEL_ICON_SIZE               24	/* Default size of panel icons */
#define PANEL_HEIGHT_DEFAULT          26	/* Default height of horizontal panel */
#define PANEL_HEIGHT_MAX              200	/* Maximum height of panel */
#define PANEL_HEIGHT_MIN              16	/* Minimum height of panel */
#define PANEL_ICON_HIGHLIGHT          0x202020	/* Constant to pass to icon loader */

struct _Panel;
typedef struct _Panel Panel;


extern GtkStyle * panel_get_default_style(Panel * p);

extern GtkWidget * panel_get_toplevel_widget(Panel * p);
extern GdkWindow * panel_get_toplevel_window(Panel * p);
extern Window      panel_get_toplevel_xwindow(Panel * p);

extern int panel_get_edge(Panel * p);
extern int panel_get_orientation(Panel * p);
extern int panel_get_oriented_height_pixels(Panel * p);

extern int panel_get_icon_size(Panel * p);


extern int verbose;

extern FbEv *fbev;

extern void panel_apply_icon(GtkWindow *w);
extern void panel_draw_label_text(Panel * p, GtkWidget * label, char * text, gboolean bold, gboolean custom_color);
extern void panel_image_set_from_file(Panel * p, GtkWidget * image, char * file);
extern gboolean panel_image_set_icon_theme(Panel * p, GtkWidget * image, const gchar * icon);

extern int panel_handle_x_error(Display * d, XErrorEvent * ev);
extern int panel_handle_x_error_swallow_BadWindow_BadDrawable(Display * d, XErrorEvent * ev);

extern const char* lxpanel_get_logout_command();
extern const char* lxpanel_get_file_manager();
extern const char* lxpanel_get_terminal();

int lxpanel_is_in_kiosk_mode(void);

#ifdef _LXPANEL_INTERNALS
extern void panel_calculate_position(Panel *p);
extern void update_panel_geometry(Panel* p);
extern void panel_adjust_geometry_terminology(Panel *p);
extern void panel_determine_background_pixmap(Panel * p, GtkWidget * widget, GdkWindow * window);
extern void panel_establish_autohide(Panel *p);
extern void panel_set_dock_type(Panel *p);
extern void panel_set_panel_configuration_changed(Panel *p);
extern void panel_update_background(Panel* p);
extern void panel_autohide_conditions_changed(Panel* p);
extern void panel_require_update_background(Panel* p);

/* to check if we are in LXDE */
extern gboolean is_in_lxde;

extern gchar *cprofile;

#endif

#endif
