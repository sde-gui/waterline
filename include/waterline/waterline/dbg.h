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

#include <sde-utils.h>

#define DBG2(fmt, args...) fprintf(stderr, "%s:%s:%-5d: " fmt, __FILE__,  __FUNCTION__, __LINE__, ## args)
#define ENTER2          do { fprintf(stderr, "%s:%s:%-5d: ENTER\n",  __FILE__,__FUNCTION__, __LINE__); } while(0)
#define RET2(args...)   do { fprintf(stderr, "%s:%s:%-5d: RETURN\n",  __FILE__,__FUNCTION__, __LINE__);\
return args; } while(0)

#ifdef DEBUG

#define ENTER          do { log_message(LOG_DBG, "%s:%s:%-5d: ENTER\n",  __FILE__,__FUNCTION__, __LINE__); } while(0)
#define RET(args...)   do { log_message(LOG_DBG, "%s:%s:%-5d: RETURN\n", __FILE__, __FUNCTION__, __LINE__);\
return args; } while(0)

#else

#define ENTER         do {  } while(0)
#define RET(args...)   return args

#endif


