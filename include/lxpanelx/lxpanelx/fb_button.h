/**
 * Copyright (c) 2013 Vadim Ushakov
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

#ifndef _LXPANELX_FBBUTTON_H
#define _LXPANELX_FBBUTTON_H

#include "typedef.h"

GtkWidget * fb_button_new_from_file(
    gchar * image_file, int width, int height, gboolean highlighted);
GtkWidget * fb_button_new_from_file_with_label(
    gchar * image_file, int width, int height, gboolean highlighted, Panel * panel, gchar * label);
void fb_button_set_orientation(GtkWidget * btn, GtkOrientation orientation);
void fb_button_set_from_file(GtkWidget* btn, const char* img_file, gint width, gint height);
void fb_button_set_label(GtkWidget * btn, Panel * panel, gchar * label);


#endif
