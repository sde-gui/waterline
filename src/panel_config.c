/**
 *
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
#include "config.h"
#endif

#include <waterline/global.h>
#include "plugin_internal.h"
#include "plugin_private.h"
#include <waterline/panel.h>
#include "panel_internal.h"
#include "panel_private.h"
#include <waterline/paths.h>
#include <waterline/misc.h>
#include <waterline/defaultapplications.h>
#include "bg.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <glib/gi18n.h>

static void save_global_config();

/******************************************************************************/

static su_enum_pair align_pair[] = {
    { ALIGN_NONE, "none" },
    { ALIGN_LEFT, "left" },
    { ALIGN_RIGHT, "right" },
    { ALIGN_CENTER, "center"},
    { 0, NULL },
};

su_enum_pair edge_pair[] = {
    { EDGE_NONE, "none" },
    { EDGE_LEFT, "left" },
    { EDGE_RIGHT, "right" },
    { EDGE_TOP, "top" },
    { EDGE_BOTTOM, "bottom" },
    { 0, NULL },
};

static su_enum_pair width_pair[] = {
    { WIDTH_NONE, "none" },
    { WIDTH_REQUEST, "request" },
    { WIDTH_PIXEL, "pixel" },
    { WIDTH_PERCENT, "percent" },
    { 0, NULL },
};
/*
static su_enum_pair height_pair[] = {
    { HEIGHT_NONE, "none" },
    { HEIGHT_PIXEL, "pixel" },
    { 0, NULL },
};

static su_enum_pair pos_pair[] = {
    { POS_NONE, "none" },
    { POS_START, "start" },
    { POS_END,  "end" },
    { 0, NULL},
};
*/
static su_enum_pair panel_visibility_pair[] = {
    { VISIBILITY_ALWAYS, "always" },
    { VISIBILITY_BELOW, "below" },
    { VISIBILITY_AUTOHIDE,  "autohide" },
    { VISIBILITY_GOBELOW,  "gobelow" },
    { 0, NULL},
};

static su_enum_pair output_target_pair[] = {
    { OUTPUT_WHOLE_SCREEN, "whole_screen" },
    { OUTPUT_PRIMARY_MONITOR, "primary_monitor" },
    { OUTPUT_CUSTOM_MONITOR,  "custom_monitor" },
    { 0, NULL},
};

static su_enum_pair background_mode_pair[] = {
    { BACKGROUND_SYSTEM, "system" },
    { BACKGROUND_IMAGE, "image" },
    { BACKGROUND_COLOR,  "color" },
    { 0, NULL},
};

/******************************************************************************/

#define SU_JSON_OPTION_STRUCTURE Panel
static su_json_option_definition option_definitions[] = {

    SU_JSON_OPTION_ENUM(edge_pair, edge), 
    SU_JSON_OPTION_ENUM(align_pair, align),
    SU_JSON_OPTION(int, edge_margin),
    SU_JSON_OPTION(int, align_margin),

    SU_JSON_OPTION_ENUM(width_pair, oriented_width_type),
    SU_JSON_OPTION(int, oriented_width),
    SU_JSON_OPTION(int, oriented_height),

    SU_JSON_OPTION_ENUM(output_target_pair, output_target),
    SU_JSON_OPTION(int, custom_monitor),

    SU_JSON_OPTION(bool, round_corners),
    SU_JSON_OPTION(int, round_corners_radius),

    SU_JSON_OPTION_ENUM(background_mode_pair, background_mode),
    SU_JSON_OPTION(string, background_file),
    SU_JSON_OPTION(int, alpha),
    SU_JSON_OPTION(color, background_color),
    SU_JSON_OPTION(bool, stretch_background),

    SU_JSON_OPTION2(string, widget_name, "GtkWidgetName"),

    SU_JSON_OPTION_ENUM(panel_visibility_pair, visibility_mode),
    SU_JSON_OPTION(int, height_when_hidden),
    SU_JSON_OPTION(int, set_strut),

    SU_JSON_OPTION(bool, use_font_color),
    SU_JSON_OPTION(color, font_color),
    SU_JSON_OPTION(bool, use_font_size),
    SU_JSON_OPTION(int, font_size),

    SU_JSON_OPTION2(int, preferred_icon_size, "icon_size"),

    SU_JSON_OPTION(int, padding_top),
    SU_JSON_OPTION(int, padding_bottom),
    SU_JSON_OPTION(int, padding_left),
    SU_JSON_OPTION(int, padding_right),
    SU_JSON_OPTION(int, applet_spacing),

    {0,}
};

/******************************************************************************/

void panel_read_global_configuration_from_json_object(Panel *p)
{
    json_t * json = json_object_get(p->json, "global");

    if (!json_is_object(json))
        return;

    su_json_read_options(json, option_definitions, p);

    if (p->alpha > 255)
        p->alpha = 255;
}

/******************************************************************************/

static void panel_write_global_configuration_to_json_object(Panel *p)
{
    if (wtl_is_in_kiosk_mode())
        return;

    json_t * json = json_incref(json_object_get(p->json, "global"));
    if (!json)
    {
        json = json_object();
        json_object_set_nocheck(p->json, "global", json);
    }

    su_json_write_options(json, option_definitions, p);

    json_decref(json);
}

static void panel_write_plugins_configuration_to_json_object(Panel* p)
{
    if (wtl_is_in_kiosk_mode())
        return;

    json_t * json_plugins = json_array();

    GList * l;
    for (l = p->plugins; l; l = l->next)
    {
        Plugin * plugin = (Plugin *) l->data;

        su_json_dot_set_string(plugin->json, "type", plugin->class->type);
        su_json_dot_set_bool(plugin->json, "expand", plugin->expand);
        su_json_dot_set_int(plugin->json, "padding", plugin->padding);
        su_json_dot_set_int(plugin->json, "border", plugin->border);

        if (plugin->class->save)
            plugin->class->save(plugin);

        json_array_append(json_plugins, plugin->json);
    }

    json_object_set_nocheck(p->json, "plugins", json_plugins);
    json_decref(json_plugins);
}

static void panel_write_configuration_to_json_object(Panel* p)
{
    panel_write_global_configuration_to_json_object(p);
    panel_write_plugins_configuration_to_json_object(p);
}

/******************************************************************************/

static int copyfile(char *src, char *dest)
{
    FILE * in = NULL;
    FILE * out = NULL;
    char * buffer = NULL;
    int len = 0;;
    int buff_size = 1024 * 8;
    int result = 0;

    buffer = malloc(buff_size);
    if (!buffer)
        goto ret;

    in = fopen(src, "r");
    if (!in)
        goto ret;

    out = fopen(dest,"w");
    if (!out)
        goto ret;

    result = 1;

    while ( (len = fread(buffer, 1, buff_size, in)) > 0 )
    {
        int l = fwrite(buffer, 1, len, out);
        if (l != len)
        {
            result = 0;
            break;
        }
    }

ret:

    if (in)
        fclose(in);
    if (out)
        fclose(out);
    if (buffer)
        free(buffer);

    return result;
}


void panel_save_configuration(Panel* p)
{
    if (wtl_is_in_kiosk_mode())
        return;

    FILE * fp = NULL;

    gchar * dir = wtl_get_config_path("panels", SU_PATH_CONFIG_USER_W);

    gchar * file_name = g_strdup_printf("%s" PANEL_FILE_SUFFIX, p->name);
    gchar * file_path = g_build_filename( dir, file_name, NULL );

    gchar * bak_file_name = g_strdup_printf("%s" PANEL_FILE_SUFFIX ".bak", p->name);
    gchar * bak_file_path = g_build_filename( dir, bak_file_name, NULL );

    /* ensure the 'panels' dir exists */
    if( ! g_file_test( dir, G_FILE_TEST_EXISTS ) )
        g_mkdir_with_parents( dir, 0755 );
    g_free( dir );

    if (g_file_test( file_path, G_FILE_TEST_EXISTS ) )
    {
        su_log_debug("copying %s => %s\n", file_path, bak_file_path);
        int r = copyfile(file_path, bak_file_path);
        if (!r)
        {
            su_print_error_message("can't save .bak file %s:", bak_file_path);
            perror(NULL);
            goto err;
        }
    }

    su_log_debug("saving panel %s to %s\n", p->name, file_path);

    if (!(fp = fopen(file_path, "w")))
    {
        su_print_error_message("can't open for write %s:", file_path);
        perror(NULL);
        goto err;
    }

    panel_write_configuration_to_json_object(p);

    if (json_dumpf(p->json, fp, JSON_INDENT(2) | JSON_PRESERVE_ORDER) != 0)
    {
        su_print_error_message("failed to write panel configuration: %s\n", file_path);
        goto err;
    }

    fclose(fp);

    g_free( file_path );
    g_free( file_name );
    g_free( bak_file_path );
    g_free( bak_file_name );

    /* save the global config file */
    save_global_config();
    p->config_changed = 0;

    return;

err:
    if (fp)
        fclose(fp);
    g_free( file_path );
    g_free( file_name );
    g_free( bak_file_path );
    g_free( bak_file_name );
}

const char general_group[] = "General";
const char command_group[] = "Command";
void load_global_config()
{
    GKeyFile* kf = g_key_file_new();
    gchar * file = wtl_get_config_path("config", SU_PATH_CONFIG_USER);
    gboolean loaded = g_key_file_load_from_file(kf, file, 0, NULL);
    g_free(file);

    if (loaded)
    {
        if (g_key_file_has_key(kf, general_group, "KioskMode", NULL))
            global_config.kiosk_mode = g_key_file_get_boolean(kf, general_group, "KioskMode", NULL);
        else
            global_config.kiosk_mode = 0;

        global_config.file_manager_cmd = g_key_file_get_string(kf, command_group, "FileManager", NULL);
        global_config.terminal_cmd     = g_key_file_get_string(kf, command_group, "Terminal", NULL);
        global_config.logout_cmd       = g_key_file_get_string(kf, command_group, "Logout", NULL);

        su_log_debug("FileManager = %s\n", global_config.file_manager_cmd);
        su_log_debug("Terminal    = %s\n", global_config.terminal_cmd);
        su_log_debug("Logout      = %s\n", global_config.logout_cmd);
        su_log_debug("KioskMode   = %s\n", global_config.kiosk_mode ? "true" : "false");
    }
    g_key_file_free(kf);
}

static void save_global_config()
{
    if (wtl_is_in_kiosk_mode())
        return;

    gchar * file = wtl_get_config_path("config", SU_PATH_CONFIG_USER_W);

    FILE * f = fopen( file, "w" );
    if (f)
    {
        fprintf( f, "[%s]\n", general_group );
        fprintf( f, "KioskMode=%s\n", global_config.kiosk_mode ? "true" : "false" );

        fprintf( f, "[%s]\n", command_group );
        if( global_config.file_manager_cmd )
            fprintf( f, "FileManager=%s\n", global_config.file_manager_cmd );
        if( global_config.terminal_cmd )
            fprintf( f, "Terminal=%s\n", global_config.terminal_cmd );
        if( global_config.logout_cmd )
            fprintf( f, "Logout=%s\n", global_config.logout_cmd );
        fclose( f );
    }

    g_free(file);
}

void free_global_config()
{
    g_free( global_config.file_manager_cmd );
    g_free( global_config.terminal_cmd );
    g_free( global_config.logout_cmd );
}

/******************************************************************************/

extern const char* wtl_get_logout_command()
{
    return (!su_str_empty(global_config.logout_cmd)) ?
        global_config.logout_cmd : wtl_get_default_application("logout");
}

extern const char* wtl_get_file_manager_application()
{
    return (!su_str_empty(global_config.file_manager_cmd)) ?
        global_config.file_manager_cmd : wtl_get_default_application("file-manager");
}

extern const char* wtl_get_terminal_emulator_application()
{
    return (!su_str_empty(global_config.terminal_cmd)) ?
        global_config.terminal_cmd : wtl_get_default_application("terminal-emulator");
}

/******************************************************************************/

extern int wtl_is_in_kiosk_mode(void)
{
    return global_config.kiosk_mode || global_config.arg_kiosk_mode;
}

extern void enable_kiosk_mode(void)
{
    global_config.arg_kiosk_mode = 1;
}
