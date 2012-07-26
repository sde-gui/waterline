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

#define _LXPANEL_INTERNALS

#include "defaultapplications.h"
#include "paths.h"
#include "dbg.h"

/********************************************************************/

static GHashTable * default_applications = NULL;

/********************************************************************/

char * get_default_application(char * type)
{
    if (!default_applications)
        default_applications = g_hash_table_new(NULL, g_str_equal);
    if (!default_applications)
        return NULL;

    gchar * result = g_hash_table_lookup(default_applications, type);
    if (result)
        return result;

    gchar * file_name = g_build_filename("applications", type, NULL);
    if (!file_name)
        return NULL;

    gchar * path = get_config_path(file_name, CONFIG_USER);
    if (!path)
        return NULL;
    g_free(file_name);

    char * data;
    g_file_get_contents(path, &data, NULL, NULL);
    if (!data)
        return NULL;

    /* Split into lines and check each line. */

    gchar ** lines = g_strsplit(data, "\n", 0);
    g_free(data);

    gchar ** l;
    for (l = lines; *l; l++)
    {
        /* Split into words and check if command present in the system. */
        gchar ** words = g_strsplit_set(*l, "\t ", 0);
        gchar ** w;
        for (w = words; *w; w++)
        {
            gchar * word = *w;
            if (!word) /* empty line */
                break;
amp_removed:
            if (*word == '#') /* comment, ignore that line */
                break;
            if (*word == 0) /* empty word just means two or more adjacent separators */
                continue;
            if (*word == '&') /* ignore '&' */
            {
                word++;
                goto amp_removed;
            }

            gchar * program_path = g_find_program_in_path(word);
            g_free(program_path);
            g_strfreev(words);

            if (program_path)
            {
                result = g_strdup(*l);
                g_strfreev(lines);
                g_hash_table_insert(default_applications, type, result);
                return result;
            }

            break;
        }// for words

    }// for lines

    g_strfreev(lines);

    return NULL;
}

/********************************************************************/

