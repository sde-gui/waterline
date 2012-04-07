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

/* to check if we are in LXDE */
extern gboolean is_in_lxde;

/* Context of a panel on a given edge. */
typedef struct _Panel {
    char* name;
    GtkWidget * topgwin;		/* Main panel window */
    Window topxwin;			/* Main panel's X window   */
    GdkDisplay * display;		/* Main panel's GdkDisplay */
    GtkStyle * defstyle;

    GtkWidget * box;			/* Top level widget */

    GtkRequisition requisition;
    GtkWidget *(*my_box_new) (gboolean, gint);
    GtkWidget *(*my_separator_new) ();

    FbBg *bg;
    int alpha;
    guint32 tintcolor;
    guint32 fontcolor;
    GdkColor gtintcolor;
    GdkColor gfontcolor;
    guint fontsize;
    int round_corners_radius;

    int ax, ay, aw, ah;  /* prefferd allocation of a panel */
    int cx, cy, cw, ch;  /* current allocation (as reported by configure event) allocation */
    int allign, edge, margin;
    int orientation;
    int widthtype, width;
    int heighttype, height;
    gulong strut_size;			/* Values for WM_STRUT_PARTIAL */
    gulong strut_lower;
    gulong strut_upper;
    int strut_edge;

    guint config_changed : 1;
    guint self_destroy : 1;
    guint setdocktype : 1;
    guint setstrut : 1;
    guint round_corners : 1;
    guint usefontcolor : 1;
    guint usefontsize : 1;
    guint transparent : 1;
    guint background : 1;
    guint spacing;

    guint autohide : 1;                 /* Autohide mode */
    guint autohide_visible : 1;         /* whether panel is in full-size state. Always true if autohide is false */
    guint visible : 1;                  /* whether panel is actually visible */
    int height_when_hidden;
    guint hide_timeout;
    int icon_size;			/* Icon size */

    int desknum;
    int curdesk;
    guint32 *workarea;
    int wa_len;

    char* background_file;

    GList * plugins;			/* List of all plugins */
    GSList * system_menus;		/* List of plugins having menus */

    GtkWidget* plugin_pref_dialog;	/* Plugin preference dialog */
    GtkWidget* pref_dialog;		/* preference dialog */
    GtkWidget* margin_control;		/* Margin control in preference dialog */
    GtkWidget* height_label;		/* Label of height control */
    GtkWidget* width_label;		/* Label of width control */
    GtkWidget* alignment_left_label;	/* Label of alignment: left control */
    GtkWidget* alignment_right_label;	/* Label of alignment: right control */
    GtkWidget* height_control;		/* Height control in preference dialog */
    GtkWidget* width_control;		/* Width control in preference dialog */

    int preferred_icon_size;

    int update_background_idle_cb;

    int set_wm_strut_idle;
} Panel;


extern gchar *cprofile;

extern int verbose;

extern FbEv *fbev;

extern void panel_apply_icon(GtkWindow *w);
extern void panel_destroy(Panel *p);
extern void panel_adjust_geometry_terminology(Panel *p);
extern void panel_determine_background_pixmap(Panel * p, GtkWidget * widget, GdkWindow * window);
extern void panel_draw_label_text(Panel * p, GtkWidget * label, char * text, gboolean bold, gboolean custom_color);
extern void panel_establish_autohide(Panel *p);
extern void panel_image_set_from_file(Panel * p, GtkWidget * image, char * file);
extern gboolean panel_image_set_icon_theme(Panel * p, GtkWidget * image, const gchar * icon);
extern void panel_set_wm_strut(Panel *p);
extern void panel_set_dock_type(Panel *p);
extern void panel_set_panel_configuration_changed(Panel *p);
extern void panel_update_background( Panel* p );

extern void panel_autohide_conditions_changed( Panel* p );

extern void panel_require_update_background( Panel* p );

extern int panel_handle_x_error(Display * d, XErrorEvent * ev);
extern int panel_handle_x_error_swallow_BadWindow_BadDrawable(Display * d, XErrorEvent * ev);

extern const char* lxpanel_get_logout_command();
extern const char* lxpanel_get_file_manager();
extern const char* lxpanel_get_terminal();

int lxpanel_is_in_kiosk_mode(void);

#endif
