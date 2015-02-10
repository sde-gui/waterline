/**
 * Copyright (c) 2011-2012 Vadim Ushakov
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <waterline/paths.h>
#include <waterline/global.h>

/********************************************************************/

extern gchar *cprofile;

/********************************************************************/

static gchar * _wtl_resolve_resource(const char * first_part, gboolean private, va_list ap)
{
    gchar * result = NULL;
    gchar * suffix = su_build_filename_va(ap);

    if (private)
        result = su_path_resolve_resource(wtl_agent_id(), first_part, "waterline", suffix, NULL);
    else
        result = su_path_resolve_resource(wtl_agent_id(), first_part, suffix, NULL);

    g_free(suffix);
    return result;
}

gchar * wtl_resolve_resource(const char * first_part, ...)
{
    gchar * result;
    va_list ap;
    va_start(ap, first_part);
    result = _wtl_resolve_resource(first_part, FALSE, ap);
    va_end(ap);
    return result;
}

gchar * wtl_resolve_own_resource(const char * first_part, ...)
{
    gchar * result;
    va_list ap;
    va_start(ap, first_part);
    result = _wtl_resolve_resource(first_part, TRUE, ap);
    va_end(ap);
    return result;
}

/********************************************************************/

#define TEMPLATE_PROFILE "template"

gchar * wtl_get_config_path(const char* file_name, SU_PATH_CONFIG_TYPE config_type)
{
    return su_path_resolve_config(wtl_agent_id(), config_type, "sde/waterline" , cprofile, file_name, NULL);
}

/********************************************************************/

