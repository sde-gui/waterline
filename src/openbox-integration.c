/**
 * Copyright (c) 2012 vadim Ushakov
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

#include <gtk/gtk.h>
#include <lxpanelx/Xsupport.h>
#include <lxpanelx/misc.h>

gchar * ob_get_current_theme(void)
{
    if (!check_net_supported(a_OB_THEME))
        return NULL;

    return get_utf8_property(GDK_ROOT_WINDOW(), a_OB_THEME);
}

gchar * ob_find_file_for_theme(const char * name, const char * target_file_name)
{
    if (name[0] == '/')
    {
        gchar * s = g_build_filename(name, "openbox-3", target_file_name, NULL);
        if (g_file_test(s, G_FILE_TEST_EXISTS))
            return s;
        g_free(s);
        return NULL;
    }

    gchar * s = g_build_filename(g_get_home_dir(), ".themes", name, "openbox-3", target_file_name, NULL);
    if (g_file_test(s, G_FILE_TEST_EXISTS))
        return s;
    g_free(s);

    const gchar * const * dirs = g_get_system_config_dirs();

    for (; *dirs; dirs++)
    {
        gchar * result = g_build_filename(*dirs, "themes", name, "openbox-3", target_file_name, NULL);
        if (g_file_test(result, G_FILE_TEST_EXISTS))
            return result;
        g_free(result);
    }

    return NULL;
}

GdkPixbuf * ob_load_icon_from_theme(const char * name, int w, int h)
{
    GdkPixbuf * result = NULL;
    gchar * file_name = NULL;
    gchar * theme = ob_get_current_theme();
    if (theme)
        file_name = ob_find_file_for_theme(theme, name);
    if (file_name)
        result = lxpanel_load_icon(file_name, w, h, FALSE);
    g_free(file_name);
    g_free(theme);
    return result;
}


