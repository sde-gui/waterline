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

#ifndef _LXPANELX_CONFIG_PARSER_H
#define _LXPANELX_CONFIG_PARSER_H

#include <gtk/gtk.h>
#include <stdio.h>

#include <jansson.h>

#include "gtkcompat.h"

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

extern pair bool_pair[];

int str2num(const pair *_p, const char * str, int defval);
const char * num2str(const pair * p, int num, const char * defval);

extern int lxpanel_get_line(char **fp, line *s);
extern int lxpanel_put_line(FILE* fp, const char* format, ...);
extern int lxpanel_put_str( FILE* fp, const char* name, const char* val );
extern int lxpanel_put_bool( FILE* fp, const char* name, gboolean val );
extern int lxpanel_put_int( FILE* fp, const char* name, int val );
extern int lxpanel_put_enum( FILE* fp, const char* name, int val, const pair* pair);

int get_line_as_is(char **fp, line *s);



int wtl_json_dot_get_enum(json_t * json, const char * key, const pair * pairs, int default_value);
int wtl_json_dot_get_int(json_t * json, const char * key, int default_value);
gboolean wtl_json_dot_get_bool(json_t * json, const char * key, gboolean default_value);
void wtl_json_dot_get_color(json_t * json, const char * key, const GdkColor * default_value, GdkColor * result);
void wtl_json_dot_get_rgba(json_t * json, const char * key, const GdkRGBA * default_value, GdkRGBA * result);
void wtl_json_dot_get_string(json_t * json, const char * key, const char * default_value, char ** result);

void wtl_json_dot_set_enum(json_t * json, const char * key, const pair * pairs, int value);
void wtl_json_dot_set_int(json_t * json, const char * key, int value);
void wtl_json_dot_set_bool(json_t * json, const char * key, gboolean value);
void wtl_json_dot_set_color(json_t * json, const char * key, const GdkColor * value);
void wtl_json_dot_set_rgba(json_t * json, const char * key, const GdkRGBA * value);
void wtl_json_dot_set_string(json_t * json, const char * key, const char * value);


typedef enum {
    wtl_json_type_enum,
    wtl_json_type_int,
    wtl_json_type_bool,
    wtl_json_type_color,
    wtl_json_type_rgba,
    wtl_json_type_string
} wtl_json_type_t;

typedef struct {
    wtl_json_type_t type;
    void * structure_offset;
    const char * key;
    const pair * pairs;
} wtl_json_option_definition;

#define WTL_JSON_OPTION(type, name) \
    { wtl_json_type_##type, (void *) &(((WTL_JSON_OPTION_STRUCTURE *) NULL)->name), #name, NULL}
#define WTL_JSON_OPTION_ENUM(pairs, name) \
    { wtl_json_type_enum, (void *) &(((WTL_JSON_OPTION_STRUCTURE *) NULL)->name), #name, pairs}
#define WTL_JSON_OPTION2(type, name, key) \
    { wtl_json_type_##type, (void *) &(((WTL_JSON_OPTION_STRUCTURE *) NULL)->name), key, NULL}
#define WTL_JSON_OPTION2_ENUM(pairs, name, key) \
    { wtl_json_type_enum, (void *) &(((WTL_JSON_OPTION_STRUCTURE *) NULL)->name), key, pairs}

void wtl_json_read_options(json_t * json, wtl_json_option_definition * options, void * structure);
void wtl_json_write_options(json_t * json, wtl_json_option_definition * options, void * structure);

/******************************************************************************/

#ifndef json_array_foreach
    #define json_array_foreach(array, index, value) \
        for(index = 0; \
            index < json_array_size(array) && (value = json_array_get(array, index)); \
            index++)
#endif

/******************************************************************************/

#endif
