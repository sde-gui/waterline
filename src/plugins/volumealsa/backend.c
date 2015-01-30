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

/******************************************************************************/

#include "backend.h"

/******************************************************************************/

static gboolean notify_state_changed(volume_control_backend_t * backend)
{

    if (backend->frontend_callback_state_changed)
        (backend->frontend_callback_state_changed)(backend->frontend, backend);
}

volume_control_backend_t * volume_control_backend_new(void)
{
    volume_control_backend_t * backend = g_new0(volume_control_backend_t, 1);
    backend->notify_state_changed = notify_state_changed;
}

#define CHECK_SANITY_FN(fn) (\
    backend && \
    backend->impl && \
    backend->impl_vtable && \
    (backend->impl_vtable->fn))

gboolean volume_control_backend_is_valid(volume_control_backend_t * backend)
{
    if (CHECK_SANITY_FN(is_valid))
        return backend->impl_vtable->is_valid(backend);
    return FALSE;
}

gboolean volume_control_backend_has_mute(volume_control_backend_t * backend)
{
    if (CHECK_SANITY_FN(has_mute))
        return backend->impl_vtable->has_mute(backend);
    return FALSE;
}

gboolean volume_control_backend_is_muted(volume_control_backend_t * backend)
{
    if (CHECK_SANITY_FN(is_muted))
        return backend->impl_vtable->is_muted(backend);
    return FALSE;
}

void volume_control_backend_set_mute(volume_control_backend_t * backend, gboolean value)
{
    if (CHECK_SANITY_FN(set_mute))
        backend->impl_vtable->set_mute(backend, value);
}

long volume_control_backend_get_volume(volume_control_backend_t * backend)
{
    if (CHECK_SANITY_FN(get_volume))
        return backend->impl_vtable->get_volume(backend);
    return FALSE;
}

void volume_control_backend_set_volume(volume_control_backend_t * backend, long volume)
{
    if (CHECK_SANITY_FN(set_volume))
        backend->impl_vtable->set_volume(backend, volume);
}

void volume_control_backend_free(volume_control_backend_t * backend)
{
    if (CHECK_SANITY_FN(destroy))
        backend->impl_vtable->destroy(backend);
    g_free(backend);
}

