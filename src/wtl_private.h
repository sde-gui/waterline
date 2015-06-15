/**
 * Copyright (c) 2015 Vadim Ushakov
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

#ifndef WTL_PRIVATE_H
#define WTL_PRIVATE_H

#include <glib.h>
#include <waterline/typedef.h>
#include <waterline/symbol_visibility.h>

extern SYMBOL_HIDDEN gboolean wtl_fm_init(void);

extern SYMBOL_HIDDEN void wtl_restart(void);

extern SYMBOL_HIDDEN void wtl_load_global_config(void);
extern SYMBOL_HIDDEN void wtl_free_global_config(void);
extern SYMBOL_HIDDEN void wtl_enable_kiosk_mode(void);

extern SYMBOL_HIDDEN gboolean wtl_x11_is_composite_available(void);
extern SYMBOL_HIDDEN void wtl_x11_update_net_supported(void);

extern SYMBOL_HIDDEN char * wtl_profile;

#endif
