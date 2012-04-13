/**
 * Copyright (c) 2012 Vadim Ushakov
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

#ifndef PANEL_PRIVATE_H
#define PANEL_PRIVATE_H

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
    int cx, cy, cw, ch;  /* current allocation (as reported by configure event) */
    int allign, edge, margin;
    int orientation;
    int oriented_width_type, oriented_width;
    int oriented_height_type, oriented_height;
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

#endif
