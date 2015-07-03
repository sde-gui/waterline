/**
 * Copyright (c) 2011-2012 Vadim Ushakov
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

#ifndef __WATERLINE__PLUGIN_PRIVATE_H
#define __WATERLINE__PLUGIN_PRIVATE_H

#include <waterline/plugin.h>

struct _PluginClassInternal {
    char * fname;               /* Plugin file pathname */
    int count;                  /* Reference count */
    GModule * gmodule;          /* Associated GModule structure */
};

/* Representative of a loaded and active plugin attached to a panel. */
struct _Plugin {
    PluginClass * class;        /* Back pointer to plugin class */
    Panel * panel;              /* Back pointer to Panel */
    GtkWidget * pwid;           /* Top level widget; plugin allocates, but plugin mechanism, not plugin itself, destroys this */
    gpointer priv;              /* Private context for plugin; plugin frees this in its destructor */

    int expand;                 /* Expand ("stretch") setting for container */
    int padding;                /* Padding setting for container */
    int border;                 /* Border setting for container */

    gboolean has_system_menu;

    GtkAllocation pwid_allocation;
    gboolean background_update_scheduled;
    int lock_visible;

    json_t * json;
};

/* FIXME: optional definitions */
#define STATIC_SEPARATOR
#define STATIC_LAUNCHBAR
#define STATIC_LAUNCHBUTTON
#define STATIC_DCLOCK
#define STATIC_WINCMD
#define STATIC_DIRMENU
#define STATIC_PAGER
#define STATIC_MENU
#define STATIC_SPACE
#define STATIC_ICONS

#endif
