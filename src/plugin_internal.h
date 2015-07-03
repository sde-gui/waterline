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

#ifndef __WATERLINE__PLUGIN_INTERNAL_H
#define __WATERLINE__PLUGIN_INTERNAL_H

#include <waterline/plugin.h>

//#pragma GCC visibility push(hidden)

extern Plugin * plugin_load(const char * type);     /* Create an instance of a plugin, loading it if necessary */
extern int plugin_start(Plugin * this);             /* Configure and start a plugin by calling its constructor */
extern void plugin_unload(Plugin * pl);             /* Delete an instance of a plugin if initialization fails */
extern void plugin_delete(Plugin * pl);             /* Delete an instance of a plugin */
extern GList * plugin_get_available_classes(void);  /* Get a list of all plugin classes; free with plugin_class_list_free */
extern void plugin_class_list_free(GList * list);   /* Free the list allocated by plugin_get_available_classes */
extern void plugin_widget_set_background(GtkWidget * w, Panel * p); /* Recursively set the background of all widgets on a panel background configuration change */

//#pragma GCC visibility pop


#endif
