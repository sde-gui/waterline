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
#include <sde-utils-jansson.h>
#include <waterline/typedef.h>
#include <waterline/symbol_visibility.h>
#include "bg.h"

enum { BACKGROUND_SYSTEM, BACKGROUND_IMAGE, BACKGROUND_COLOR };

struct _Panel {
    char* name;
    GtkWidget * topgwin;
    Window topxwin;
    GdkDisplay * display;
    GdkScreen * screen;
    GtkStyle * defstyle;

    GtkWidget * toplevel_alignment; /* Widget containing plugin_box */
    GtkWidget * plugin_box;         /* Widget containing plugins */

    GtkRequisition requisition;
    GtkWidget *(*my_box_new) (gboolean, gint);

    char * widget_name;

    gboolean is_composited;
    guint composite_check_timeout;

    FbBg *bg;

    int background_mode;
    int alpha;
    GdkColor background_color;

    GdkColor font_color;

    int ax, ay, aw, ah;  /* prefferd allocation of a panel */
    int cx, cy, cw, ch;  /* current allocation (as reported by configure event) */
    int align, edge, edge_margin, align_margin;
    int orientation;
    int oriented_width_type, oriented_width;
    int oriented_height_type, oriented_height;

    int output_target;
    int custom_monitor;

    /* Set by calculate_position(), used by configurator.c to adjust spin buttons. */
    int output_target_width;
    int output_target_height;

    int padding_top;
    int padding_bottom;
    int padding_left;
    int padding_right;
    int applet_spacing;

    int preferred_icon_size;

    gulong strut_size; /* Values for WM_STRUT_PARTIAL */
    gulong strut_lower;
    gulong strut_upper;
    int strut_edge;
    int set_wm_strut_idle;

    int visibility_mode;

    gboolean config_changed;
    gboolean self_destroy;
    gboolean set_strut;
    gboolean use_font_color;
    gboolean use_font_size;
    int font_size;
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

    gboolean doing_panel_drag_move;
    int panel_drag_move_start_x;
    int panel_drag_move_start_y;
    int panel_drag_move_start_edge_margin;
    int panel_drag_move_start_align_margin;

    GList * plugins; /* List of all plugins */

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

        GtkSpinButton * custom_monitor;

        GtkSpinButton * padding_top;
        GtkSpinButton * padding_bottom;
        GtkSpinButton * padding_left;
        GtkSpinButton * padding_right;
        GtkSpinButton * applet_spacing;

        GtkWidget * preferred_applications_file_manager;
        GtkWidget * preferred_applications_terminal_emulator;
        GtkWidget * preferred_applications_logout;
        GtkWidget * preferred_applications_info_label;

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

/* panel_config.c */

extern void panel_save_configuration(Panel* panel);

extern void SYMBOL_HIDDEN panel_read_global_configuration_from_json_object(Panel *p);

/* configurator.c */

extern void panel_configure(Panel* p, int sel_page );
extern void configurator_remove_plugin_from_list(Panel * p, Plugin * pl);

extern SYMBOL_HIDDEN void create_empty_panel(void);
extern SYMBOL_HIDDEN void delete_panel(Panel * panel);
extern SYMBOL_HIDDEN int panel_count(void);

extern gboolean quit_in_menu;

extern SYMBOL_HIDDEN const char * wtl_license;
extern SYMBOL_HIDDEN const char * wtl_website;
extern SYMBOL_HIDDEN const char * wtl_email ;
extern SYMBOL_HIDDEN const char * wtl_bugreporting;

extern SYMBOL_HIDDEN su_enum_pair edge_pair[];

#define PANEL_FILE_SUFFIX ".js"

#endif
