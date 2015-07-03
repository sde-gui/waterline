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

#ifndef __WATERLINE__PANEL_INTERNAL_H
#define __WATERLINE__PANEL_INTERNAL_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "config.h"

#include <waterline/typedef.h>
#include <waterline/symbol_visibility.h>

#define PANEL_HEIGHT_MAX              200      /* Maximum height of panel */
#define PANEL_HEIGHT_MIN              16       /* Minimum height of panel */
#define PANEL_ICON_HIGHLIGHT          0x202020 /* Constant to pass to icon loader */

//#pragma GCC visibility push(hidden)

extern void panel_update_geometry(Panel* p);
extern void panel_set_panel_configuration_changed(Panel *p);
extern void panel_update_background(Panel* p);
extern SYMBOL_HIDDEN void panel_autohide_conditions_changed(Panel* p);
extern SYMBOL_HIDDEN void panel_require_update_background(Panel* p);

extern SYMBOL_HIDDEN int panel_get_icon_size(Panel * p);
extern SYMBOL_HIDDEN int panel_get_orientation(Panel * p);

extern SYMBOL_HIDDEN GtkMenu * panel_get_panel_menu(Panel * panel, Plugin * plugin);
extern SYMBOL_HIDDEN void panel_show_panel_menu(Panel * panel, Plugin * plugin, GdkEventButton * event);

extern SYMBOL_HIDDEN void panel_button_press_hack(Panel *panel);
extern SYMBOL_HIDDEN gboolean panel_handle_drag_move(Panel * panel, GdkEventButton * event);

//#pragma GCC visibility pop

#endif
