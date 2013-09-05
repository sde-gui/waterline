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

#include <jansson.h>
#include <waterline/typedef.h>
#include <waterline/configparser.h>
#include "bg.h"


struct _Panel {
    char* name;
    GtkWidget * topgwin;		/* Main panel window */
    Window topxwin;			/* Main panel's X window   */
    GdkDisplay * display;		/* Main panel's GdkDisplay */
    GtkStyle * defstyle;

    GtkWidget * toplevel_alignment;	/* Widget containing plugin_box */
    GtkWidget * plugin_box;			/* Widget containing plugins */

    GtkRequisition requisition;
    GtkWidget *(*my_box_new) (gboolean, gint);

    char * widget_name;

    FbBg *bg;
    int alpha;
    GdkColor tint_color;
    GdkColor font_color;

    int round_corners_radius;

    int ax, ay, aw, ah;  /* prefferd allocation of a panel */
    int cx, cy, cw, ch;  /* current allocation (as reported by configure event) */
    int align, edge, edge_margin, align_margin;
    int orientation;
    int oriented_width_type, oriented_width;
    int oriented_height_type, oriented_height;

    int padding_top;
    int padding_bottom;
    int padding_left;
    int padding_right;
    int applet_spacing;

    int preferred_icon_size;

    gulong strut_size;			/* Values for WM_STRUT_PARTIAL */
    gulong strut_lower;
    gulong strut_upper;
    int strut_edge;
    int set_wm_strut_idle;

    int visibility_mode;

    gboolean config_changed;
    gboolean self_destroy;
    gboolean set_strut;
    gboolean round_corners;
    gboolean use_font_color;
    gboolean use_font_size;
    int font_size;
    gboolean transparent;
    gboolean background;
    guint spacing;

    gboolean gobelow;
    gboolean autohide_visible;         /* whether panel is in full-size state. Always true if autohide is false */
    gboolean visible;                  /* whether panel is actually visible */
    int height_when_hidden;
    guint hide_timeout;

    int desknum;
    int curdesk;
    guint32 *workarea;
    int wa_len;

    char* background_file;

    gboolean expose_event_connected;
    gboolean alpha_channel_support;
    gboolean rgba_transparency;
    gboolean stretch_background;

    GdkPixmap * background_pixmap;

    int update_background_idle_cb;

    GList * plugins;			/* List of all plugins */

    json_t * json;

    struct {
        GtkWidget * pref_dialog;
        GtkWidget * plugin_pref_dialog;
        GtkWidget * notebook;
        GtkWidget * alignment_left_label;
        GtkWidget * alignment_right_label;
        GtkWidget * edge_margin_label;
        GtkWidget * edge_margin_control;
        GtkWidget * align_margin_label;
        GtkWidget * align_margin_control;
        GtkWidget * width_label;
        GtkWidget * width_control;
        GtkWidget * width_unit;
        GtkWidget * height_label;
        GtkWidget * height_control;

        GtkWidget * always_visible;
        GtkWidget * always_below;
        GtkWidget * autohide;
        GtkWidget * gobelow;
        GtkWidget * height_when_minimized;
        GtkWidget * reserve_space;

        GtkSpinButton * padding_top;
        GtkSpinButton * padding_bottom;
        GtkSpinButton * padding_left;
        GtkSpinButton * padding_right;
        GtkSpinButton * applet_spacing;

        gboolean doing_update;
    } pref_dialog;
};

struct {
    char * file_manager_cmd;
    char * terminal_cmd;
    char * logout_cmd;
    int kiosk_mode;
    int arg_kiosk_mode;
} global_config;

GKeyFile * global_settings;

/* panel_config.c */

extern void load_global_config(void);
extern void free_global_config(void);
extern void enable_kiosk_mode(void);
extern void panel_save_configuration(Panel* panel);

extern void panel_read_global_configuration_from_json_object(Panel *p);

/* configurator.c */

extern void panel_configure(Panel* p, int sel_page );
extern gboolean panel_edge_available(Panel* p, int edge);
extern void configurator_remove_plugin_from_list(Panel * p, Plugin * pl);

extern void create_empty_panel(void);
extern void delete_panel(Panel * panel);
extern int panel_count(void);

extern gboolean quit_in_menu;

extern const char * __license;
extern const char * __website;
extern const char * __email ;
extern const char * __bugreporting;

extern su_enum_pair edge_pair[];

#define PANEL_FILE_SUFFIX ".js"

#endif
