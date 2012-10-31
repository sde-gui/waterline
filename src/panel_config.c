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

#include <lxpanelx/global.h>
#include "plugin_internal.h"
#include "plugin_private.h"
#include <lxpanelx/panel.h>
#include "panel_internal.h"
#include "panel_private.h"
#include <lxpanelx/paths.h>
#include <lxpanelx/misc.h>
#include <lxpanelx/defaultapplications.h>
#include "bg.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <glib/gi18n.h>

#include <lxpanelx/dbg.h>

static void save_global_config();

void panel_global_config_save( Panel* p, FILE *fp);
void panel_plugin_config_save( Panel* p, FILE *fp);

/******************************************************************************/

int
panel_parse_global(Panel *p, char **fp)
{
    line s;

    if (!fp)
        return 1;

    while (lxpanel_get_line(fp, &s) != LINE_NONE) {
        if (s.type == LINE_VAR) {
            if (!g_ascii_strcasecmp(s.t[0], "edge")) {
                p->edge = str2num(edge_pair, s.t[1], EDGE_NONE);
            } else if (!g_ascii_strcasecmp(s.t[0], "align")) {
                p->align = str2num(align_pair, s.t[1], ALIGN_NONE);
            } else if (!g_ascii_strcasecmp(s.t[0], "margin")) {
                p->align_margin = atoi(s.t[1]);
            } else if (!g_ascii_strcasecmp(s.t[0], "AlignMargin")) {
                p->align_margin = atoi(s.t[1]);
            } else if (!g_ascii_strcasecmp(s.t[0], "widthtype")) {
                p->oriented_width_type = str2num(width_pair, s.t[1], WIDTH_NONE);
            } else if (!g_ascii_strcasecmp(s.t[0], "width")) {
                p->oriented_width = atoi(s.t[1]);
            } else if (!g_ascii_strcasecmp(s.t[0], "heighttype")) {
                p->oriented_height_type = str2num(height_pair, s.t[1], HEIGHT_NONE);
            } else if (!g_ascii_strcasecmp(s.t[0], "height")) {
                p->oriented_height = atoi(s.t[1]);
            } else if (!g_ascii_strcasecmp(s.t[0], "spacing")) {
                p->spacing = atoi(s.t[1]);
            } else if (!g_ascii_strcasecmp(s.t[0], "SetDockType")) {
                /* ignore */
            } else if (!g_ascii_strcasecmp(s.t[0], "SetPartialStrut")) {
                p->setstrut = str2num(bool_pair, s.t[1], 0);
            } else if (!g_ascii_strcasecmp(s.t[0], "RoundCorners")) {
                p->round_corners = str2num(bool_pair, s.t[1], 0);
            } else if (!g_ascii_strcasecmp(s.t[0], "RoundCornersRadius")) {
                p->round_corners_radius = atoi(s.t[1]);
            } else if (!g_ascii_strcasecmp(s.t[0], "Transparent")) {
                p->transparent = str2num(bool_pair, s.t[1], 0);
            } else if (!g_ascii_strcasecmp(s.t[0], "Alpha")) {
                p->alpha = atoi(s.t[1]);
                if (p->alpha > 255)
                    p->alpha = 255;
            } else if (!g_ascii_strcasecmp(s.t[0], "AutoHide")) {
                p->visibility_mode = str2num(bool_pair, s.t[1], 0) ? VISIBILITY_AUTOHIDE : VISIBILITY_ALWAYS;
            } else if (!g_ascii_strcasecmp(s.t[0], "Visibility")) {
                p->visibility_mode = str2num(panel_visibility_pair, s.t[1], 0);
            } else if (!g_ascii_strcasecmp(s.t[0], "HeightWhenHidden")) {
                p->height_when_hidden = atoi(s.t[1]);
            } else if (!g_ascii_strcasecmp(s.t[0], "TintColor")) {
                if (!gdk_color_parse (s.t[1], &p->gtintcolor))
                    gdk_color_parse ("white", &p->gtintcolor);
                p->tintcolor = gcolor2rgb24(&p->gtintcolor);
                DBG("tintcolor=%x\n", p->tintcolor);
            } else if (!g_ascii_strcasecmp(s.t[0], "UseFontColor")) {
                p->usefontcolor = str2num(bool_pair, s.t[1], 0);
            } else if (!g_ascii_strcasecmp(s.t[0], "FontColor")) {
                if (!gdk_color_parse (s.t[1], &p->gfontcolor))
                    gdk_color_parse ("black", &p->gfontcolor);
                p->fontcolor = gcolor2rgb24(&p->gfontcolor);
                DBG("fontcolor=%x\n", p->fontcolor);
            } else if (!g_ascii_strcasecmp(s.t[0], "UseFontSize")) {
                p->usefontsize = str2num(bool_pair, s.t[1], 0);
            } else if (!g_ascii_strcasecmp(s.t[0], "FontSize")) {
                p->fontsize = atoi(s.t[1]);   
            } else if (!g_ascii_strcasecmp(s.t[0], "Background")) {
                p->background = str2num(bool_pair, s.t[1], 0);
            } else if( !g_ascii_strcasecmp(s.t[0], "BackgroundFile") ) {
                p->background_file = g_strdup( s.t[1] );
            } else if (!g_ascii_strcasecmp(s.t[0], "RGBATransparency")) {
                p->rgba_transparency = str2num(bool_pair, s.t[1], 0);
            } else if (!g_ascii_strcasecmp(s.t[0], "StretchBackground")) {
                p->stretch_background = str2num(bool_pair, s.t[1], 0);
            } else if (!g_ascii_strcasecmp(s.t[0], "IconSize")) {
                p->preferred_icon_size = atoi(s.t[1]);
            } else if( !g_ascii_strcasecmp(s.t[0], "GtkWidgetName") ) {
                g_free(p->widget_name);
                p->widget_name = g_strdup( s.t[1] );
            } else {
                ERR( "lxpanel: %s - unknown var in Global section\n", s.t[0]);
            }
        } else if (s.type == LINE_BLOCK_END) {
            break;
        } else {
            ERR( "lxpanel: illegal in this context %s\n", s.str);
            return 0;
        }
    }

    return 1;
}

/******************************************************************************/

void
panel_global_config_save(Panel* p, FILE *fp)
{
    if (lxpanel_is_in_kiosk_mode())
        return;

    fprintf(fp, "# lxpanelx <profile> config file. Manually editing is not recommended.\n"
                "# Use preference dialog in lxpanelx to adjust config when you can.\n\n");
    lxpanel_put_line(fp, "Global {");
    lxpanel_put_str(fp, "Edge", num2str(edge_pair, p->edge, "none"));
    lxpanel_put_str(fp, "Align", num2str(align_pair, p->align, "none"));
    lxpanel_put_int(fp, "AlignMargin", p->align_margin);
    lxpanel_put_str(fp, "WidthType", num2str(width_pair, p->oriented_width_type, "none"));
    lxpanel_put_int(fp, "Width", p->oriented_width);
    lxpanel_put_int(fp, "Height", p->oriented_height);

    lxpanel_put_bool(fp, "RoundCorners", p->round_corners );
    lxpanel_put_int(fp, "RoundCornersRadius", p->round_corners_radius );

    lxpanel_put_bool(fp, "RGBATransparency", p->rgba_transparency);
    lxpanel_put_bool(fp, "StretchBackground", p->stretch_background);
    lxpanel_put_bool(fp, "Background", p->background );
    lxpanel_put_str(fp, "BackgroundFile", p->background_file);
    lxpanel_put_bool(fp, "Transparent", p->transparent );
    lxpanel_put_line(fp, "TintColor=#%06x", gcolor2rgb24(&p->gtintcolor));
    lxpanel_put_int(fp, "Alpha", p->alpha);

    lxpanel_put_str(fp, "GtkWidgetName", p->widget_name);

    lxpanel_put_enum(fp, "Visibility", p->visibility_mode, panel_visibility_pair);
    lxpanel_put_int(fp, "HeightWhenHidden", p->height_when_hidden);
    lxpanel_put_bool(fp, "SetPartialStrut", p->setstrut);

    lxpanel_put_bool(fp, "UseFontColor", p->usefontcolor);
    lxpanel_put_line(fp, "FontColor=#%06x", gcolor2rgb24(&p->gfontcolor));
    lxpanel_put_bool(fp, "UseFontSize", p->usefontsize);    
    lxpanel_put_int(fp, "FontSize", p->fontsize);

    lxpanel_put_int(fp, "IconSize", p->preferred_icon_size);

    lxpanel_put_line(fp, "}\n");
}

void
panel_plugin_config_save( Panel* p, FILE *fp)
{
    if (lxpanel_is_in_kiosk_mode())
        return;

    GList* l;
    for( l = p->plugins; l; l = l->next )
    {
        Plugin* pl = (Plugin*)l->data;
        lxpanel_put_line( fp, "Plugin {" );
        lxpanel_put_line( fp, "type = %s", pl->class->type );
        if( pl->expand )
            lxpanel_put_bool( fp, "expand", TRUE );
        if( pl->padding > 0 )
            lxpanel_put_int( fp, "padding", pl->padding );
        if( pl->border > 0 )
            lxpanel_put_int( fp, "border", pl->border );

        if( pl->class->save )
        {
            lxpanel_put_line( fp, "Config {" );
            pl->class->save( pl, fp );
            lxpanel_put_line( fp, "}" );
        }
        lxpanel_put_line( fp, "}\n" );
    }
}

int copyfile(char *src, char *dest)
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


void panel_config_save( Panel* p )
{
    if (lxpanel_is_in_kiosk_mode())
        return;

    FILE * fp = NULL;

    gchar * dir = get_config_path("panels", CONFIG_USER_W);

    gchar * file_name = g_strdup_printf("%s.panel", p->name);
    gchar * file_path = g_build_filename( dir, file_name, NULL );

    gchar * bak_file_name = g_strdup_printf("%s.panel.bak", p->name);
    gchar * bak_file_path = g_build_filename( dir, bak_file_name, NULL );

    /* ensure the 'panels' dir exists */
    if( ! g_file_test( dir, G_FILE_TEST_EXISTS ) )
        g_mkdir_with_parents( dir, 0755 );
    g_free( dir );

    if (g_file_test( file_path, G_FILE_TEST_EXISTS ) )
    {
        int r = copyfile(file_path, bak_file_path);
        if (!r)
        {
            ERR("can't save .bak file %s:", bak_file_path);
            perror(NULL);
            goto err;
        }
    }

    if (!(fp = fopen(file_path, "w")))
    {
        ERR("can't open for write %s:", file_path);
        perror(NULL);
        goto err;
    }

    panel_global_config_save(p, fp);
    panel_plugin_config_save(p, fp);

    fclose(fp);

    g_free( file_path );
    g_free( file_name );
    g_free( bak_file_path );
    g_free( bak_file_name );

    /* save the global config file */
    save_global_config();
    p->config_changed = 0;

    RET();

err:
    g_free( file_path );
    g_free( file_name );
    g_free( bak_file_path );
    g_free( bak_file_name );
    RET();
}

const char general_group[] = "General";
const char command_group[] = "Command";
void load_global_config()
{
    GKeyFile* kf = g_key_file_new();
    gchar * file = get_config_path("config", CONFIG_USER);
    gboolean loaded = g_key_file_load_from_file( kf, file, 0, NULL );
    g_free(file);

    if (loaded)
    {
        if (g_key_file_has_key(kf, general_group, "KioskMode", NULL))
            global_config.kiosk_mode = g_key_file_get_boolean( kf, general_group, "KioskMode", NULL );
        else
            global_config.kiosk_mode = 0;

        global_config.file_manager_cmd = g_key_file_get_string( kf, command_group, "FileManager", NULL );
        global_config.terminal_cmd     = g_key_file_get_string( kf, command_group, "Terminal", NULL );
        global_config.logout_cmd       = g_key_file_get_string( kf, command_group, "Logout", NULL );
    }
    g_key_file_free( kf );
}

static void save_global_config()
{
    if (lxpanel_is_in_kiosk_mode())
        return;

    gchar * file = get_config_path("config", CONFIG_USER_W);

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

extern const char* lxpanel_get_logout_command()
{
    return global_config.logout_cmd;
}

extern const char* lxpanel_get_file_manager()
{
    return global_config.file_manager_cmd ?
        global_config.file_manager_cmd : get_default_application("file-manager");
}

extern const char* lxpanel_get_terminal()
{
    return global_config.terminal_cmd ?
        global_config.terminal_cmd : get_default_application("terminal-emulator");
}

extern int lxpanel_is_in_kiosk_mode(void)
{
    return global_config.kiosk_mode || global_config.arg_kiosk_mode;
}

extern void enable_kiosk_mode(void)
{
    global_config.arg_kiosk_mode = 1;
}
