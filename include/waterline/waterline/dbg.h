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

#include <stdio.h>
#include <stdarg.h>

#define ERR(fmt, args...) print_error_message(fmt, ## args)

#define DBG2(fmt, args...) fprintf(stderr, "%s:%s:%-5d: " fmt, __FILE__,  __FUNCTION__, __LINE__, ## args)
#define ENTER2          do { fprintf(stderr, "%s:%s:%-5d: ENTER\n",  __FILE__,__FUNCTION__, __LINE__); } while(0)
#define RET2(args...)   do { fprintf(stderr, "%s:%s:%-5d: RETURN\n",  __FILE__,__FUNCTION__, __LINE__);\
return args; } while(0)

typedef enum _SU_LOG_LEVEL {
    SU_LOG_NONE,
    SU_LOG_ERROR,
    SU_LOG_WARNING,
    SU_LOG_INFO,
    SU_LOG_DEBUG,
    SU_LOG_DEBUG_SPAM,
    SU_LOG_DEBUG_2 = SU_LOG_DEBUG_SPAM,
    SU_LOG_ALL
} SU_LOG_LEVEL;

void su_log_message_va(SU_LOG_LEVEL level, const char * format, va_list ap);
void su_log_message   (SU_LOG_LEVEL level, const char * format, ...);
void su_log_error     (const char * format, ...);
void su_log_warning   (const char * format, ...);
void su_log_info      (const char * format, ...);
void su_log_debug     (const char * format, ...);
void su_log_debug2    (const char * format, ...);

void print_error_message(const char *string, ...);

#define SU_LOG_DEBUG2(fmt, args...) do { if (log_level >= SU_LOG_DEBUG_2) su_log_debug2(fmt, ## args); } while(0)

#ifdef DEBUG

#define ENTER          do { log_message(LOG_DBG, "%s:%s:%-5d: ENTER\n",  __FILE__,__FUNCTION__, __LINE__); } while(0)
#define RET(args...)   do { log_message(LOG_DBG, "%s:%s:%-5d: RETURN\n", __FILE__, __FUNCTION__, __LINE__);\
return args; } while(0)
//#define DBG(fmt, args...) log_message(LOG_DBG, "%s:%s:%-5d: " fmt,  __FILE__,__FUNCTION__, __LINE__, ## args)
//#define LOG(level, fmt, args...) log_message(level, fmt, ## args)

#else

#define ENTER         do {  } while(0)
#define RET(args...)   return args
//#define DBG(fmt, args...)   do {  } while(0)
//#define LOG(level, fmt, args...) do { if (level <= log_level) log_message(level, fmt, ## args); } while(0)

#endif

extern int log_level;
