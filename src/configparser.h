/**
 * Copyright (c) 2011 Vadim Ushakov
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

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <gtk/gtk.h>
#include <stdio.h>

enum { LINE_NONE, LINE_BLOCK_START, LINE_BLOCK_END, LINE_VAR };


#define CONFIG_LINE_LENGTH 256

typedef struct {
    int num, ln, type;
    gchar str[CONFIG_LINE_LENGTH];
    gchar *t[3];
} line;

typedef struct {
    int num;
    gchar *str;
} pair;

extern pair align_pair[];
extern pair edge_pair[];
extern pair width_pair[];
extern pair height_pair[];
extern pair bool_pair[];
extern pair pos_pair[];

int str2num(pair *p, gchar *str, int defval);
const gchar *num2str(const pair *p, int num, const gchar *defval);

extern int lxpanel_get_line(char **fp, line *s);
extern int lxpanel_put_line(FILE* fp, const char* format, ...);
extern int lxpanel_put_str( FILE* fp, const char* name, const char* val );
extern int lxpanel_put_bool( FILE* fp, const char* name, gboolean val );
extern int lxpanel_put_int( FILE* fp, const char* name, int val );
extern int lxpanel_put_enum( FILE* fp, const char* name, int val, const pair* pair);

int get_line_as_is(char **fp, line *s);

#endif
