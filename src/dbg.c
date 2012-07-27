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

#include <lxpanelx/dbg.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

int log_level = LOG_WARN;


#define RED_COLOR               "\033[0;31m"
#define GREEN_COLOR             "\033[0;32m"
#define ORANGE_COLOR            "\033[0;33m"
#define BLUE_COLOR              "\033[0;34m"
#define TEAL_COLOR              "\033[0;36m"
#define LIGHT_YELLOW_COLOR      "\033[1;33m"
#define LIGHT_BLUE_COLOR        "\033[1;34m"
#define LIGHT_PURPLE_COLOR      "\033[1;35m"
#define NORMAL_COLOR            "\033[0m"


void log_message(int level, const char *string, ...)
{
        char *modifier = "";
        FILE *stream = stderr;
        switch (level)
        {
                case LOG_ERR:
                {
                        modifier = RED_COLOR "[ERR] " NORMAL_COLOR;
                        stream = stderr;
                        break;
                }
                case LOG_WARN:
                {
                        modifier = ORANGE_COLOR "[WRN] " NORMAL_COLOR;
                        break;
                }
                case LOG_INFO:
                {
                        modifier = GREEN_COLOR "[INF] " NORMAL_COLOR;
                        break;
                }
                case LOG_DBG:
                {
                        modifier = TEAL_COLOR "[DBG] " NORMAL_COLOR;
                        break;
                }
        }

        time_t curtime = time(NULL);
        struct tm *loctime = localtime(&curtime);
        char *time_buffer = malloc(256 * sizeof(char));
        time_buffer[0] = 0;
        if (loctime != NULL)
        {
                char *tb = malloc(256 * sizeof(char));
                strftime(tb, 256, "%T", loctime);
                sprintf(time_buffer, "%s ", tb);
                free(tb);
        }

        char *f = "%s%s%s\n";
        int string_len = strlen(string);
        if (string_len > 0 && string[string_len - 1] == '\n')
            f = "%s%s%s";

        int len = strlen(string) + strlen(modifier) + strlen(time_buffer) + 3;

        char *buffer = (char *) malloc(len + 1);

        snprintf(buffer, len, f, modifier, time_buffer, string);

        buffer[len] = 0;

        va_list ap;
        va_start(ap, string);

        vfprintf(stream, buffer, ap);

        free(buffer);
        free(time_buffer);
        va_end(ap);
}
