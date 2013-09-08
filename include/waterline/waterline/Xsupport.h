/**
 * Copyright (c) 2011 Vadim Ushakov
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

#ifndef __WATERLINE__XSUPPORT_H
#define __WATERLINE__XSUPPORT_H

#include <X11/X.h>

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


void Xclimsg(Window win, Atom type, long l0, long l1, long l2, long l3, long l4);
void Xclimsgwm(Window win, Atom type, Atom arg);
void *get_xaproperty (Window win, Atom prop, Atom type, int *nitems);
char *get_textproperty(Window win, Atom prop);
void *get_utf8_property(Window win, Atom atom);
char **get_utf8_property_list(Window win, Atom atom, int *count);

void resolve_atoms();
//Window Select_Window(Display *dpy);
int get_net_number_of_desktops();
int get_net_current_desktop ();
int get_net_wm_desktop(Window win);
void set_net_wm_desktop(Window win, int num);
int get_wm_state (Window win);
void get_net_wm_state(Window win, NetWMState *nws);
void get_net_wm_window_type(Window win, NetWMWindowType *nwwt);
GPid get_net_wm_pid(Window win);

void set_decorations (Window win, gboolean decorate);
int get_mvm_decorations(Window win);
gboolean get_decorations (Window win, NetWMState * nws);

void update_net_supported();
gboolean check_net_supported(Atom atom);

gboolean is_xcomposite_available(void);

void wm_noinput(Window w);

GdkPixbuf * get_wm_icon(Window task_win, int required_width, int required_height, Atom source, Atom * current_source);

#define A(x) extern Atom a##x;
#include "Xsupport_atoms.h"
#undef A

#define a_NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define a_NET_WM_STATE_ADD           1    /* add/set property */
#define a_NET_WM_STATE_TOGGLE        2    /* toggle property  */

/* if current window manager is EWMH conforming. */
extern gboolean is_ewmh_supported;

#endif
