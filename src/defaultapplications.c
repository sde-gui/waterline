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

#include <fnmatch.h>
#include <sde-utils.h>

#include <waterline/defaultapplications.h>
#include <waterline/paths.h>
#include <waterline/dbg.h>
#include <waterline/misc.h>

/********************************************************************/

static GHashTable * default_applications = NULL;

/********************************************************************/

gchar ** read_list_from_config(gchar * file_name)
{
    gchar * path = wtl_get_config_path(file_name, SU_PATH_CONFIG_USER);
    if (!path)
        return NULL;

    gchar ** lines = su_read_lines_from_file(path, SU_READ_LIST_IGNORE_COMMENTS | SU_READ_LIST_IGNORE_WHITESPACES);

    g_free(path);

    return lines;
}

static const char * wtl_get_default_application_special_cases(char * type)
{
    if (g_strcmp0(type, "logout") == 0) {
        /* If LXSession is running, _LXSESSION_PID will be set */
        if (getenv("_LXSESSION_PID"))
            return "lxsession-logout";
    }

    return NULL;
}


const char * wtl_get_default_application(char * type)
{
    {
        const char * result = wtl_get_default_application_special_cases(type);
        if (result)
            return result;
    }

    {
        if (!default_applications)
            default_applications = g_hash_table_new(NULL, g_str_equal);
        if (!default_applications)
            return NULL;

        gchar * result = g_hash_table_lookup(default_applications, type);
        if (result)
            return result;
    }

    const char * de_name = get_de_name();

again: ;

    gchar * file_name = g_build_filename("applications", type, NULL);
    if (!file_name)
        return NULL;

    gchar ** lines = read_list_from_config(file_name);
    g_free(file_name);
    if (!lines)
        return NULL;

    gchar ** l;
    for (l = lines; *l; l++)
    {
        gchar * line = *l;

        while(g_ascii_isspace(*line)) /* skip leading whitespaces */
            line++;

        if (*line == '#' || *line == 0) /* ignore comments and empty lines */
            continue;

        if (*line == '[') { /* DE specifier */
            gchar * close_bracket_position = g_strstr_len(line, -1, "]");
            if (!close_bracket_position) /* invalid syntax, ignore line */
                continue;
            gchar * pattern = line + 1;
            *close_bracket_position = 0;
            line = close_bracket_position + 1;
            if (!su_str_empty(de_name)) {
                if (fnmatch(pattern, de_name, 0) != 0)
                    continue;
            }
        }
        else {
            if (!su_str_empty(de_name)) /* skip line without a DE specifier when running the first (DE-matching) pass */
                continue;
        }

        /* Split into words and check if command present in the system. */
        gchar ** words = g_strsplit_set(line, "\t ", 0);
        gchar ** w;
        for (w = words; *w; w++)
        {
            gchar * word = *w;
            if (!word) /* empty line */
                break;

            while (*word == '&') /* skip '&' */
                word++;

            if (*word == 0) /* empty word due to redundant whitespaces, skip it*/
                continue;

            gchar * program_path = g_find_program_in_path(word);
            g_free(program_path);

            if (program_path)
            {
                g_strfreev(words);
                gchar * result = g_strdup(line);
                g_strfreev(lines);
                g_hash_table_insert(default_applications, type, result);
                return result;
            }

            break;
        }// for words

        g_strfreev(words);

    }// for lines

    g_strfreev(lines);

    if (!su_str_empty(de_name)) {
        de_name = NULL;
        goto again;
    }

    return NULL;
}

/********************************************************************/

