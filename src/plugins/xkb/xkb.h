/*
//====================================================================
//  xfce4-xkb-plugin - XFCE4 Xkb Layout Indicator panel plugin
// -------------------------------------------------------------------
//  Alexander Iliev <sasoiliev@mamul.org>
//  20-Feb-04
// -------------------------------------------------------------------
//  Parts of this code belong to Michael Glickman <wmalms@yahooo.com>
//  and his program wmxkb.
//  WARNING: DO NOT BOTHER Michael Glickman WITH QUESTIONS ABOUT THIS
//           PROGRAM!!! SEND INSTEAD EMAILS TO <sasoiliev@mamul.org>
//====================================================================
*/

/* Modified by Hong Jen Yee (PCMan) <pcman.tw@gmail.com> on 2008-04-06 for lxpanel */

#ifndef _XKB_PLUGIN_H_
#define _XKB_PLUGIN_H_

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <gtk/gtk.h>
#include <glib.h>

#define PLUGIN_PRIV_TYPE XkbPlugin

#include <waterline/global.h>
#include <waterline/plugin.h>
#include <waterline/misc.h>
#include <waterline/paths.h>
#include <waterline/panel.h>
#include <waterline/ev.h>

#include <waterline/dbg.h>

typedef struct {

    /* Plugin interface. */
    Plugin * plugin;				/* Back pointer to Plugin */
    GtkWidget * btn;				/* Top level button */
    GtkWidget * label;				/* Label containing country name */
    GtkWidget * image;				/* Image containing country flag */
    gboolean display_as_text;			/* Display layout as image or text */
    gboolean per_window_layout;			/* Enable per application (window) layout) */
    gint default_group;				/* Default group for "locale per process" */
    guint source_id;				/* Source ID for channel listening to XKB events */
    GtkWidget * config_dlg;			/* Configuration dialog */
    GtkWidget * per_app_default_layout_menu;	/* Combo box of all available layouts */

    /* Mechanism. */
    int base_event_code;			/* Result of initializing Xkb extension */
    int base_error_code;
    int current_group_xkb_no;			/* Current layout */
    int group_count;				/* Count of groups as returned by Xkb */
    char * group_names[XkbNumKbdGroups];	/* Group names as returned by Xkb */
    char * symbol_names[XkbNumKbdGroups];	/* Symbol names as returned by Xkb */
    GHashTable * group_hash_table;		/* Hash table to correlate application with layout */

} XkbPlugin;

extern void xkb_redraw(XkbPlugin * xkb);

extern int xkb_get_current_group_xkb_no(XkbPlugin * xkb);
extern int xkb_get_group_count(XkbPlugin * xkb);
extern const char * xkb_get_symbol_name_by_res_no(XkbPlugin * xkb, int group_res_no);
extern const char * xkb_get_current_group_name(XkbPlugin * xkb);
extern const char * xkb_get_current_symbol_name(XkbPlugin * xkb);
extern const char * xkb_get_current_symbol_name_lowercase(XkbPlugin * xkb);
extern void xkb_mechanism_constructor(XkbPlugin * xkb);
extern void xkb_mechanism_destructor(XkbPlugin * xkb);
extern int xkb_change_group(XkbPlugin * xkb, int increment);
extern void xkb_active_window_changed(XkbPlugin * xkb, Window window);

#endif

