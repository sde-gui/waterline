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

extern GdkPixbuf * ob_load_icon_from_theme(const char * name, int w, int h);

void load_window_action_icon(GtkImage * image, const char * name, GtkIconSize icon_size)
{
    int w, h;
    gtk_icon_size_lookup(icon_size, &w, &h);
#if 0
    const char * openbox_icon_name = NULL;

    if (strcmp(name, "close") == 0)
        openbox_icon_name = "close.xbm";
    else if (strcmp(name, "iconify") == 0)
        openbox_icon_name = "iconify.xbm";
    else if (strcmp(name, "maximize") == 0)
        openbox_icon_name = "max.xbm";
    else if (strcmp(name, "restore") == 0)
        openbox_icon_name = "max_toggled.xbm";
    else if (strcmp(name, "shade") == 0)
        openbox_icon_name = "shade.xbm";

    if (openbox_icon_name)
    {
        GdkPixbuf * icon = ob_load_icon_from_theme(openbox_icon_name, w, h);
        if (icon)
        {
            gtk_image_set_from_pixbuf(image, icon);
            g_object_unref(icon);
            return;
        }
    }
#endif

    const char * xfce_icon_name = NULL;

    if (strcmp(name, "close") == 0)
        xfce_icon_name = "xfce-wm-close";
    else if (strcmp(name, "iconify") == 0)
        xfce_icon_name = "xfce-wm-minimize";
    else if (strcmp(name, "maximize") == 0)
        xfce_icon_name = "xfce-wm-maximize";
    else if (strcmp(name, "restore") == 0)
        xfce_icon_name = "xfce-wm-unmaximize";
    else if (strcmp(name, "shade") == 0)
        xfce_icon_name = "xfce-wm-shade";

    if (xfce_icon_name)
    {
        GdkPixbuf * icon = lxpanel_load_icon(xfce_icon_name, w, h, FALSE);
        if (icon)
        {
            gtk_image_set_from_pixbuf(image, icon);
            g_object_unref(icon);
            return;
        }
    }

    if (strcmp(name, "close") == 0)
        gtk_image_set_from_stock(image, GTK_STOCK_CLOSE, icon_size);
}

