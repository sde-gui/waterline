/**
 * Copyright (c) 2011-2015 Vadim Ushakov
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <sde-utils.h>

#include <waterline/defaultapplications.h>
#include <waterline/launch.h>

/********************************************************************/

char* wtl_translate_exec_to_cmd(const char * exec, const char * icon,
                            const char * title, const char * fpath)
{
    if (!exec)
        return NULL;

    if (!title)
        title = "";
    if (!fpath)
        fpath = "";

    GString* cmd = g_string_sized_new( 256 );
    for( ; *exec; ++exec )
    {
        if( G_UNLIKELY(*exec == '%') )
        {
            ++exec;
            if( !*exec )
                break;
            switch( *exec )
            {
                case 'c':
                    g_string_append( cmd, title );
                    break;
                case 'i':
                    if( icon )
                    {
                        g_string_append( cmd, "--icon " );
                        g_string_append( cmd, icon );
                    }
                    break;
                case 'k':
                {
                    char* uri = g_filename_to_uri( fpath, NULL, NULL );
                    g_string_append( cmd, uri );
                    g_free( uri );
                    break;
                }
                case '%':
                    g_string_append_c( cmd, '%' );
                    break;
            }
        }
        else
            g_string_append_c( cmd, *exec );
    }
    return g_string_free( cmd, FALSE );
}

/********************************************************************/

gboolean wtl_launch(const char* command, GList* files)
{
    if (!command)
        return FALSE;

    while (*command == ' ' || *command == '\t')
        command++;

    int use_terminal = FALSE;

    if (*command == '&')
        use_terminal = TRUE,
        command++;

    if (!*command)
        return FALSE;

    return wtl_launch_app(command, files, use_terminal);
}

gboolean wtl_launch_app(const char* exec, GList* files, gboolean in_terminal)
{
    GError *error = NULL;
    char* cmd;
    if( ! exec )
        return FALSE;
    cmd = su_translate_app_exec_to_command_line(exec, files);
    if( in_terminal )
    {
        char* term_cmd;
        const char * term = wtl_get_terminal_emulator_application();
        if (!term)
        {
            g_free(cmd);
            return FALSE;
        }

        char * escaped_cmd = g_shell_quote(cmd);

        if( strstr(term, "%s") )
            term_cmd = g_strdup_printf(term, escaped_cmd);
        else
            term_cmd = g_strconcat( term, " -e ", escaped_cmd, NULL );
        g_free(escaped_cmd);
        if( cmd != exec )
            g_free(cmd);
        cmd = term_cmd;
    }

    //g_print("%s\n", cmd);

    if (! g_spawn_command_line_async(cmd, &error) ) {
        su_print_error_message("can't spawn %s\nError is %s\n", cmd, error->message);
        g_error_free (error);
    }
    g_free(cmd);

    return (error == NULL);
}

/********************************************************************/

gchar * wtl_translate_directory_name(const gchar * name)
{
    gchar * title = NULL;

    if ( name )
    {
        /* load the name from *.directory file if needed */
        if ( g_str_has_suffix( name, ".directory" ) )
        {
            GKeyFile* kf = g_key_file_new();
            char* dir_file = g_build_filename( "desktop-directories", name, NULL );
            if ( g_key_file_load_from_data_dirs( kf, dir_file, NULL, 0, NULL ) )
            {
                title = g_key_file_get_locale_string( kf, "Desktop Entry", "Name", NULL, NULL );
            }
            g_free( dir_file );
            g_key_file_free( kf );
        }
    }

    if ( !title )
        title = g_strdup(name);

    return title;
}

/********************************************************************/

/* Open a specified path in a file manager. */
void wtl_open_in_file_manager(const char * path)
{
    char * quote = g_shell_quote(path);
    const char * fm = wtl_get_file_manager_application();
    char * cmd = ((strstr(fm, "%s") != NULL) ? g_strdup_printf(fm, quote) : g_strdup_printf("%s %s", fm, quote));
    g_free(quote);
    g_spawn_command_line_async(cmd, NULL);
    g_free(cmd);
}

/* Open a specified path in a terminal. */
void wtl_open_in_terminal(const char * path)
{
    const char * term = wtl_get_terminal_emulator_application();
    char * argv[2];
    char * sp = strchr(term, ' ');
    argv[0] = ((sp != NULL) ? g_strndup(term, sp - term) : (char *) term);
    argv[1] = NULL;
    g_spawn_async(path, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
    if (argv[0] != term)
        g_free(argv[0]);
}

void wtl_open_web_link(const char * link)
{
    gchar * addr = NULL;
    if (strchr(link, ':'))
        addr = g_strdup(link);
    else
        addr = g_strdup_printf("http://%s", link);

    wtl_open_in_file_manager(addr);
    g_free(addr);
}

/********************************************************************/
