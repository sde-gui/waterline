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

#ifndef _LXPANELX_PATHS_H
#define _LXPANELX_PATHS_H

#include <gtk/gtk.h>

typedef enum _RESOURCE_TYPE
{
    RESOURCE_LIB,
    RESOURCE_LIBEXEC,
    RESOURCE_DATA,
    RESOURCE_LOCALE
} RESOURCE_TYPE;

extern gchar * get_resource_path(RESOURCE_TYPE restype, ...);
extern gchar * get_private_resource_path(RESOURCE_TYPE restype, ...);


typedef enum _CONFIG_TYPE
{
    CONFIG_SYSTEM,
    CONFIG_USER,
    CONFIG_USER_W
} CONFIG_TYPE;

extern gchar * get_config_path(const char * file_name, CONFIG_TYPE config_type);

#include "typedef.h"

/* Placed here to minimize dependencies between headers. */
extern gchar * plugin_get_config_path(Plugin * plugin, const char * file_name, CONFIG_TYPE config_type);


#endif
