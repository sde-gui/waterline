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

#ifndef _LXPANELX_PANEL_INTERNAL_H
#define _LXPANELX_PANEL_INTERNAL_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "config.h"

#include <lxpanelx/typedef.h>

#define PANEL_HEIGHT_MAX              200	/* Maximum height of panel */
#define PANEL_HEIGHT_MIN              16	/* Minimum height of panel */
#define PANEL_ICON_HIGHLIGHT          0x202020	/* Constant to pass to icon loader */

#pragma GCC visibility push(hidden)

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

extern Plugin * panel_get_plugin_by_name(Panel* p, const gchar * name);

extern int panel_get_icon_size(Panel * p);
extern int panel_get_orientation(Panel * p);

extern GtkMenu * panel_get_panel_menu(Panel * panel, Plugin * plugin, gboolean use_sub_menu);
extern void panel_show_panel_menu(Panel * panel, Plugin * plugin, GdkEventButton * event);

#pragma GCC visibility pop

#endif
