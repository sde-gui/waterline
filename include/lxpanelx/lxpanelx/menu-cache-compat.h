/**
 * Copyright (c) 2012 Vadim Ushakov
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

#ifndef MENU_CACHE_COMPAT_H
#define MENU_CACHE_COMPAT_H

#include <menu-cache.h>

#define MODULE_CHECK_VERSION(module,major,minor,micro)    \
    (module##_MAJOR_VERSION > (major) || \
    (module##_MAJOR_VERSION == (major) && module##_MINOR_VERSION > (minor)) || \
    (module##_MAJOR_VERSION == (major) && module##_MINOR_VERSION == (minor) && \
     module##_MICRO_VERSION >= (micro)))

#ifndef MENU_CACHE_CHECK_VERSION
#define MENU_CACHE_CHECK_VERSION(major,minor,micro) MODULE_CHECK_VERSION(MENU_CACHE,major,minor,micro)
#endif

#ifndef MENU_CACHE_MAJOR_VERSION
#warning "MENU_CACHE_MAJOR_VERSION not defined"
#endif

#ifndef MENU_CACHE_MINOR_VERSION
#warning "MENU_CACHE_MINOR_VERSION not defined"
#endif

#ifndef MENU_CACHE_MICRO_VERSION
#warning "MENU_CACHE_MICRO_VERSION not defined"
#endif




#if !MENU_CACHE_CHECK_VERSION(0,4,0)
typedef gpointer MenuCacheReloadNotify;
#endif

#endif
