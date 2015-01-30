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

#include <glib.h>
#include <waterline/misc.h>

#ifndef WATERLINE_PLUGIN_VOLUME_CONTROL_BACKEND_H
#define WATERLINE_PLUGIN_VOLUME_CONTROL_BACKEND_H

typedef struct volume_control_backend volume_control_backend_t;
typedef struct volume_control_backend_impl_vtable volume_control_backend_impl_vtable_t;
typedef void (*volume_control_backend_impl_new)(volume_control_backend_t * backend);

#ifndef BACKEND_IMPLEMENTATION
typedef void (*frontend_callback_state_changed_t)(gpointer frontend, gpointer backend);
#endif

struct volume_control_backend_impl_vtable {
    gboolean (*is_valid)(volume_control_backend_t * backend);
    gboolean (*has_mute)(volume_control_backend_t * backend);
    gboolean (*is_muted)(volume_control_backend_t * backend);
    void     (*set_mute)(volume_control_backend_t * backend, gboolean value);
    long     (*get_volume)(volume_control_backend_t * backend);
    void     (*set_volume)(volume_control_backend_t * backend, long volume);
    void     (*destroy)(volume_control_backend_t * backend);
};

struct volume_control_backend {
    volume_control_backend_impl_vtable_t * impl_vtable;
    gpointer impl;
    gboolean (*notify_state_changed)(volume_control_backend_t * backend);
#ifndef BACKEND_IMPLEMENTATION
    gpointer frontend;
    frontend_callback_state_changed_t frontend_callback_state_changed;
#endif
};

#ifndef BACKEND_IMPLEMENTATION
extern SYMBOL_HIDDEN volume_control_backend_t * volume_control_backend_new(void);
extern SYMBOL_HIDDEN gboolean volume_control_backend_is_valid(volume_control_backend_t * backend);
extern SYMBOL_HIDDEN gboolean volume_control_backend_has_mute(volume_control_backend_t * backend);
extern SYMBOL_HIDDEN gboolean volume_control_backend_is_muted(volume_control_backend_t * backend);
extern SYMBOL_HIDDEN void volume_control_backend_set_mute(volume_control_backend_t * backend, gboolean value);
extern SYMBOL_HIDDEN long volume_control_backend_get_volume(volume_control_backend_t * backend);
extern SYMBOL_HIDDEN void volume_control_backend_set_volume(volume_control_backend_t * backend, long volume);
extern SYMBOL_HIDDEN void volume_control_backend_free(volume_control_backend_t * backend);

/*extern SYMBOL_HIDDEN gboolean volume_control_backend_set_frontend(volume_control_backend_t * backend, gpointer frontend);
extern SYMBOL_HIDDEN gboolean volume_control_backend_set_frontend_callback_state_changed(
    volume_control_backend_t * backend, frontend_callback_state_changed_t);*/

#endif

#ifdef BACKEND_IMPLEMENTATION
inline static void volume_control_backend_notify_state_changed(volume_control_backend_t * backend)
{
    if (backend && backend->notify_state_changed)
        backend->notify_state_changed(backend);
}
#endif


#endif /* WATERLINE_PLUGIN_VOLUME_CONTROL_BACKEND_H */
