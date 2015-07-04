/**
 * Copyright (c) 2015 Vadim Ushakov
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

#ifndef __WATERLINE__WTL_BUTTON_H
#define __WATERLINE__WTL_BUTTON_H

#include "typedef.h"

extern GtkWidget * wtl_button_new(Plugin * plugin);
extern GtkWidget * wtl_button_new_from_image_name(Plugin * plugin, gchar * image_name, int size);
extern GtkWidget * wtl_button_new_from_image_name_with_text(Plugin * plugin, gchar * image_name, int size, const char * text);
extern GtkWidget * wtl_button_new_from_image_name_with_markup(Plugin * plugin, gchar * image_name, int size, const char * markup);

extern void wtl_button_set_orientation(GtkWidget * button, GtkOrientation orientation);
extern void wtl_button_set_image_name(GtkWidget * button, const char * image_name, int size);

extern void wtl_button_set_label_text(GtkWidget * button, const char * text);
extern void wtl_button_set_label_markup(GtkWidget * button, const char * markup);

#endif
