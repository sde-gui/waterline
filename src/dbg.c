/**
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

#include <waterline/dbg.h>

#include <glib.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

int log_level = SU_LOG_WARNING;


#define RED_COLOR               "\033[0;31m"
#define GREEN_COLOR             "\033[0;32m"
#define ORANGE_COLOR            "\033[0;33m"
#define BLUE_COLOR              "\033[0;34m"
#define TEAL_COLOR              "\033[0;36m"
#define LIGHT_YELLOW_COLOR      "\033[1;33m"
#define LIGHT_BLUE_COLOR        "\033[1;34m"
#define LIGHT_PURPLE_COLOR      "\033[1;35m"
#define NORMAL_COLOR            "\033[0m"


void su_log_message_va(SU_LOG_LEVEL level, const char * format, va_list ap)
{
    if (level > log_level)
        return;

    FILE * stream = stderr;
    const char * modifier = "";
    switch (level)
    {
            case SU_LOG_ERROR:
            {
                    modifier = RED_COLOR "[ERR] " NORMAL_COLOR;
                    break;
            }
            case SU_LOG_WARNING:
            {
                    modifier = ORANGE_COLOR "[WRN] " NORMAL_COLOR;
                    break;
            }
            case SU_LOG_INFO:
            {
                    modifier = GREEN_COLOR "[INF] " NORMAL_COLOR;
                    break;
            }
            case SU_LOG_DEBUG:
            {
                    modifier = TEAL_COLOR "[DBG] " NORMAL_COLOR;
                    break;
            }
            case SU_LOG_DEBUG_SPAM:
            {
                    modifier = BLUE_COLOR "[DBG] " NORMAL_COLOR;
                    break;
            }
            default:
            {
                    modifier = RED_COLOR "[XXX] " NORMAL_COLOR;
                    break;
            }
    }

    time_t current_time = time(NULL);
    struct tm * local_time = localtime(&current_time);
    char time_buffer[256];
    time_buffer[0] = 0;
    if (local_time != NULL)
    {
            strftime(time_buffer, 256, "%Y-%m-%d %T ", local_time);
    }

    const char * f = "%s%s["LIGHT_YELLOW_COLOR"%s"NORMAL_COLOR"] %s\n";
    int format_len = strlen(format);
    if (format_len > 0 && format[format_len - 1] == '\n')
        f = "%s%s["LIGHT_YELLOW_COLOR"%s"NORMAL_COLOR"] %s";

    const char * program_name = g_get_prgname();

    int len = strlen(format) + strlen(modifier) + strlen(time_buffer) + strlen(program_name) + 20;

    char * buffer = (char *) malloc(len + 1);

    snprintf(buffer, len, f, modifier, time_buffer, program_name, format);

    buffer[len] = 0;

    vfprintf(stream, buffer, ap);

    free(buffer);
}

/********************************************************************/

void su_log_message(SU_LOG_LEVEL level, const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    su_log_message_va(level, format, ap);
    va_end(ap);
}

/********************************************************************/

void su_log_error(const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    su_log_message_va(SU_LOG_ERROR, format, ap);
    va_end(ap);
}

void su_log_warning(const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    su_log_message_va(SU_LOG_WARNING, format, ap);
    va_end(ap);
}

void su_log_info(const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    su_log_message_va(SU_LOG_INFO, format, ap);
    va_end(ap);
}

void su_log_debug(const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    su_log_message_va(SU_LOG_DEBUG, format, ap);
    va_end(ap);
}

void su_log_debug2(const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    su_log_message_va(SU_LOG_DEBUG_SPAM, format, ap);
    va_end(ap);
}

/********************************************************************/

void print_error_message(const char *string, ...)
{
    fprintf(stderr, "%s: ", g_get_prgname());

    va_list ap;
    va_start(ap, string);

    vfprintf(stderr, string, ap);

    va_end(ap);
}

