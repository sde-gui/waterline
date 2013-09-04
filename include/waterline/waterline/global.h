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

#ifndef __WATERLINE__GLOBAL_H
#define __WATERLINE__GLOBAL_H

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "config.h"

#include "ev.h"

extern int verbose;

extern FbEv *fbev;

extern void panel_apply_icon(GtkWindow *w);

extern int panel_handle_x_error(Display * d, XErrorEvent * ev);
extern int panel_handle_x_error_swallow_BadWindow_BadDrawable(Display * d, XErrorEvent * ev);

extern GSList * get_all_panels(void);

extern const char* wtl_get_logout_command(void);
extern const char* wtl_get_file_manager(void);
extern const char* wtl_get_terminal(void);

int wtl_is_in_kiosk_mode(void);

#endif
