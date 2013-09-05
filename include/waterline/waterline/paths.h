/**
 * Copyright (c) 2012 Vadim Ushakov
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

#ifndef __WATERLINE__PATHS_H
#define __WATERLINE__PATHS_H

#include <gtk/gtk.h>
#include <sde-utils.h>

extern gchar * wtl_resolve_resource(const char * first_part, ...);
extern gchar * wtl_resolve_own_resource(const char * first_part, ...);


extern gchar * wtl_get_config_path(const char * file_name, SU_PATH_CONFIG_TYPE config_type);

#include "typedef.h"

/* Placed here to minimize dependencies between headers. */
extern gchar * plugin_get_config_path(Plugin * plugin, const char * file_name, SU_PATH_CONFIG_TYPE config_type);


#endif
