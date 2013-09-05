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
#include <waterline/dbg.h>

/********************************************************************/

extern gchar *cprofile;

/********************************************************************/

static gchar * _get_resource_path(RESOURCE_TYPE restype, gboolean private, va_list ap)
{
    if (private && restype == RESOURCE_LOCALE)
        return NULL;

    gchar * prefix = NULL;
    switch (restype)
    {
        case RESOURCE_LIB     : prefix = PACKAGE_LIB_DIR;     break;
        case RESOURCE_LIBEXEC : prefix = PACKAGE_LIBEXEC_DIR; break;
        case RESOURCE_DATA    : prefix = PACKAGE_DATA_DIR;    break;
        case RESOURCE_LOCALE  : prefix = PACKAGE_LOCALE_DIR;  break;
    };

    if (!prefix)
        return NULL;

    char * args[16];
    int i = 0;

    args[i++] = prefix;

    if (private)
        args[i++] = "waterline";

    char * arg;
    do {
        arg = va_arg(ap, char*);
        args[i++] = arg;
        if (i >= 15)
            return NULL;
    } while (arg);

    gchar * result = g_build_filenamev(args);
//g_print("%s\n", result);
    return result;
}

gchar * get_resource_path(RESOURCE_TYPE restype, ...)
{
    gchar * result;
    va_list ap;
    va_start(ap, restype);
    result = _get_resource_path(restype, FALSE, ap);
    va_end(ap);
    return result;
}

gchar * get_private_resource_path(RESOURCE_TYPE restype, ...)
{
    gchar * result;
    va_list ap;
    va_start(ap, restype);
    result = _get_resource_path(restype, TRUE, ap);
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

