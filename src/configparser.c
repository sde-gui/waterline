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

#include "configparser.h"
#include "panel.h"
#include "dbg.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

pair align_pair[] = {
    { ALIGN_NONE, "none" },
    { ALIGN_LEFT, "left" },
    { ALIGN_RIGHT, "right" },
    { ALIGN_CENTER, "center"},
    { 0, NULL },
};

pair edge_pair[] = {
    { EDGE_NONE, "none" },
    { EDGE_LEFT, "left" },
    { EDGE_RIGHT, "right" },
    { EDGE_TOP, "top" },
    { EDGE_BOTTOM, "bottom" },
    { 0, NULL },
};

pair width_pair[] = {
    { WIDTH_NONE, "none" },
    { WIDTH_REQUEST, "request" },
    { WIDTH_PIXEL, "pixel" },
    { WIDTH_PERCENT, "percent" },
    { 0, NULL },
};

pair height_pair[] = {
    { HEIGHT_NONE, "none" },
    { HEIGHT_PIXEL, "pixel" },
    { 0, NULL },
};

pair bool_pair[] = {
    { 0, "0" },
    { 1, "1" },
    { 0, NULL },
};
pair pos_pair[] = {
    { POS_NONE, "none" },
    { POS_START, "start" },
    { POS_END,  "end" },
    { 0, NULL},
};


int
str2num(pair *_p, gchar *str, int defval)
{
    ENTER;

    pair * p;

    for (p = _p; p && p->str; p++) {
        if (!g_ascii_strcasecmp(str, p->str))
            RET(p->num);
    }

    gchar * s;
    for (s = str; *s; s++) {
        if (*s < '0' || *s > '9')
            RET(defval);
    }

    int num = atoi(str);

    for (p = _p; p && p->str; p++) {
        if (p->num == num)
            RET(p->num);
    }

    RET(defval);
}

const gchar *
num2str(const pair *p, int num, const gchar *defval)
{
    ENTER;
    for (;p && p->str; p++) {
        if (num == p->num)
            RET(p->str);
    }
    RET(defval);
}

int buf_gets( char* buf, int len, char **fp )
{
    char* p;
    int i = 0;
    if( !fp || !(p = *fp) || !**fp )
    {
        buf[0] = '\0';
        return 0;
    }

    do
    {
        if( G_LIKELY( i < len ) )
        {
            buf[i] = *p;
            ++i;
        }
        if( G_UNLIKELY(*p == '\n') )
        {
            ++p;
            break;
        }
    }while( *(++p) );
    buf[i] = '\0';
    *fp = p;
    return i;
}

extern int
lxpanel_put_line(FILE* fp, const char* format, ...)
{
    static int indent = 0;
    int i, ret;
    va_list args;

    if( strchr(format, '}') )
        --indent;

    for( i = 0; i < indent; ++i )
        fputs( "    ", fp );

    va_start (args, format);
    ret = vfprintf (fp, format, args);
    va_end (args);

    if( strchr(format, '{') )
        ++indent;
    fputc( '\n', fp );  /* add line break */
    return (ret + 1);
}

extern int
lxpanel_put_str( FILE* fp, const char* name, const char* val )
{
    if( G_UNLIKELY( !val || !*val ) )
        return 0;
    return lxpanel_put_line( fp, "%s = %s", name, val );
}

extern int
lxpanel_put_bool( FILE* fp, const char* name, gboolean val )
{
    return lxpanel_put_line( fp, "%s = %c", name, val ? '1' : '0' );
}

extern int
lxpanel_put_int( FILE* fp, const char* name, int val )
{
    return lxpanel_put_line( fp, "%s = %d", name, val );
}

extern int
lxpanel_put_enum( FILE* fp, const char* name, int val, const pair* pair)
{
    return lxpanel_put_str(fp, name, num2str(pair, val, NULL));
}

extern  int
lxpanel_get_line(char**fp, line *s)
{
    gchar *tmp, *tmp2;

    s->ln = CONFIG_LINE_LENGTH;

    s->type = LINE_NONE;
    if (!fp)
        RET(s->type);

    while (buf_gets(s->str, s->ln, fp)) {

        g_strstrip(s->str);

        if (s->str[0] == '#' || s->str[0] == 0) {
            continue;
        }
        if (!g_ascii_strcasecmp(s->str, "}")) {
            s->type = LINE_BLOCK_END;
            break;
        }

        s->t[0] = s->str;
        for (tmp = s->str; isalnum(*tmp); tmp++);
        for (tmp2 = tmp; isspace(*tmp2); tmp2++);
        if (*tmp2 == '=') {
            for (++tmp2; isspace(*tmp2); tmp2++);
            s->t[1] = tmp2;
            *tmp = 0;
            s->type = LINE_VAR;
        } else if  (*tmp2 == '{') {
            *tmp = 0;
            s->type = LINE_BLOCK_START;
        } else {
            ERR( "parser: unknown token: '%c'\n", *tmp2);
        }
        break;
    }
    return s->type;
}

int
get_line_as_is(char** fp, line *s)
{
    gchar *tmp, *tmp2;

    ENTER;

    s->ln = CONFIG_LINE_LENGTH;

    if (!fp) {
        s->type = LINE_NONE;
        RET(s->type);
    }
    s->type = LINE_NONE;
    while (buf_gets(s->str, s->ln, fp)) {
        g_strstrip(s->str);
        if (s->str[0] == '#' || s->str[0] == 0)
        continue;
        DBG( ">> %s\n", s->str);
        if (!g_ascii_strcasecmp(s->str, "}")) {
            s->type = LINE_BLOCK_END;
            DBG( "        : line_block_end\n");
            break;
        }
        for (tmp = s->str; isalnum(*tmp); tmp++);
        for (tmp2 = tmp; isspace(*tmp2); tmp2++);
        if (*tmp2 == '=') {
            s->type = LINE_VAR;
        } else if  (*tmp2 == '{') {
            s->type = LINE_BLOCK_START;
        } else {
            DBG( "        : ? <%c>\n", *tmp2);
        }
        break;
    }
    RET(s->type);

}

