/**
 * Copyright (c) 2011-2015 Vadim Ushakov
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

#ifndef __WATERLINE__X11_UTILS_H
#define __WATERLINE__X11_UTILS_H

#include <sde-utils-x11.h>
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xlib.h>

/* Decoded value of WM_STATE property. */
typedef struct {
    unsigned int modal : 1;
    unsigned int sticky : 1;
    unsigned int maximized_vert : 1;
    unsigned int maximized_horz : 1;
    unsigned int shaded : 1;
    unsigned int skip_taskbar : 1;
    unsigned int skip_pager : 1;
    unsigned int hidden : 1;
    unsigned int fullscreen : 1;
    unsigned int above : 1;
    unsigned int below : 1;
    unsigned int demands_attention : 1;
    unsigned int ob_undecorated : 1;
} NetWMState;

/* Decoded value of _NET_WM_WINDOW_TYPE property. */
typedef struct {
    unsigned int desktop : 1;
    unsigned int dock : 1;
    unsigned int toolbar : 1;
    unsigned int menu : 1;
    unsigned int utility : 1;
    unsigned int splash : 1;
    unsigned int dialog : 1;
    unsigned int normal : 1;
} NetWMWindowType;

extern void * wtl_x11_get_xa_property(Window xid, Atom prop, Atom type, int * nitems);
extern char * wtl_x11_get_utf8_property(Window win, Atom atom);
extern char * wtl_x11_get_text_property(Window win, Atom prop);
extern char ** wtl_x11_get_utf8_property_list(Window win, Atom atom, int *count);

extern void Xclimsg(Window win, Atom type, long l0, long l1, long l2, long l3, long l4);
extern void Xclimsgwm(Window win, Atom type, Atom arg);

//Window Select_Window(Display *dpy);
extern int get_net_number_of_desktops();
extern int get_net_current_desktop ();
extern int get_net_wm_desktop(Window win);
extern void set_net_wm_desktop(Window win, int num);
extern int get_wm_state (Window win);
extern void get_net_wm_state(Window win, NetWMState *nws);
extern void get_net_wm_window_type(Window win, NetWMWindowType *nwwt);
extern GPid get_net_wm_pid(Window win);

extern void set_decorations (Window win, gboolean decorate);
extern int get_mvm_decorations(Window win);
extern gboolean get_decorations (Window win, NetWMState * nws);

extern void update_net_supported();
extern gboolean check_net_supported(Atom atom);

extern gboolean is_xcomposite_available(void);

extern void wm_noinput(Window w);

extern GdkPixbuf * get_wm_icon(Window task_win, int required_width, int required_height, Atom source, Atom * current_source);

extern gboolean get_net_showing_desktop_supported(void);
extern gboolean get_net_showing_desktop(void);
extern void set_net_showing_desktop(gboolean value);

#endif
