/**
 * Copyright (c) 2013-2015 Vadim Ushakov
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

#ifndef __WATERLINE__LAUNCH_H
#define __WATERLINE__LAUNCH_H

#include <glib.h>

extern char* translate_exec_to_cmd( const char* exec, const char* icon,
                             const char* title, const char* fpath );

extern gboolean wtl_launch_app(const char* exec, GList* files, gboolean in_terminal);
extern gboolean wtl_launch(const char* exec, GList* files);
extern void wtl_open_in_file_manager(const char * path);
extern void wtl_open_in_terminal(const char * path);
extern void wtl_open_web_link(const char * link);

extern gchar * panel_translate_directory_name(const gchar * name);

#endif
