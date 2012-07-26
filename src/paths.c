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

#define _LXPANEL_INTERNALS

#include "global.h"
#include "paths.h"
#include "dbg.h"

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
        args[i++] = "lxpanelx";

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

gchar * get_config_path(const char* file_name, CONFIG_TYPE config_type)
{
    gchar * profile = cprofile;

again:

    if (config_type == CONFIG_USER || config_type == CONFIG_USER_W)
    {
        gchar * result = g_build_filename(g_get_user_config_dir(), "lxpanelx" , profile, file_name, NULL);
        if (config_type == CONFIG_USER_W)
        {
            gchar * dirname = g_path_get_dirname(result);
            if (!g_file_test(dirname, G_FILE_TEST_EXISTS))
                g_mkdir_with_parents(dirname, 0755);
            g_free(dirname);
            return result;
        }
        if (g_file_test(result, G_FILE_TEST_EXISTS))
            return result;
        g_free(result);
    }

    const gchar * const * dirs = g_get_system_config_dirs();

    for (; *dirs; dirs++)
    {
        gchar * result = g_build_filename(*dirs, "lxpanelx", profile, file_name, NULL);
        if (g_file_test(result, G_FILE_TEST_EXISTS))
            return result;
        g_free(result);
    }

    gchar * result = get_private_resource_path(RESOURCE_DATA, "profile", profile, file_name, NULL);
    if (g_file_test(result, G_FILE_TEST_EXISTS))
        return result;
    g_free(result);

    if (strcmp(profile, TEMPLATE_PROFILE) != 0)
    {
        profile = TEMPLATE_PROFILE;
        goto again;
    }

    return NULL;
}

/********************************************************************/

