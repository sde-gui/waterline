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

#include <waterline/configparser.h>
#include <waterline/misc.h>
#include <waterline/panel.h>
#include <waterline/dbg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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
wtl_put_line(FILE* fp, const char* format, ...)
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

#define VARNAME_ALIGN 24

extern int
wtl_put_str( FILE* fp, const char* name, const char* val )
{
    if( G_UNLIKELY( !val || !*val ) )
        return 0;

    if (strchr(val, '\n'))
    {
        ERR("Value of a config variable cannot contain newline character. Variable %s ignored.\n", name);
        return 0;
    }

    char s[VARNAME_ALIGN+1];
    int l = strlen(name);
    if (l < VARNAME_ALIGN)
    {
        memcpy(s, name, l);
        for (; l < VARNAME_ALIGN; l++)
            s[l] = ' ';
        s[VARNAME_ALIGN] = 0;
        name = s;
    }

    return wtl_put_line( fp, "%s = %s", name, val );
}

extern int
wtl_put_bool( FILE* fp, const char* name, gboolean val )
{
    char s[VARNAME_ALIGN+1];
    int l = strlen(name);
    if (l < VARNAME_ALIGN)
    {
        memcpy(s, name, l);
        for (; l < VARNAME_ALIGN; l++)
            s[l] = ' ';
        s[VARNAME_ALIGN] = 0;
        name = s;
    }

    return wtl_put_line( fp, "%s = %s", name, val ? "true" : "false" );
}

extern int
wtl_put_int( FILE* fp, const char* name, int val )
{
    char s[VARNAME_ALIGN+1];
    int l = strlen(name);
    if (l < VARNAME_ALIGN)
    {
        memcpy(s, name, l);
        for (; l < VARNAME_ALIGN; l++)
            s[l] = ' ';
        s[VARNAME_ALIGN] = 0;
        name = s;
    }

    return wtl_put_line( fp, "%s = %d", name, val );
}

extern int
wtl_put_enum( FILE* fp, const char* name, int val, const su_enum_pair* pair)
{
    char s[VARNAME_ALIGN+1];
    int l = strlen(name);
    if (l < VARNAME_ALIGN)
    {
        memcpy(s, name, l);
        for (; l < VARNAME_ALIGN; l++)
            s[l] = ' ';
        s[VARNAME_ALIGN] = 0;
        name = s;
    }

    return wtl_put_str(fp, name, num2str(pair, val, NULL));
}

extern  int
wtl_get_line(char**fp, line *s)
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

