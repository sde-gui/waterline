/**
 * Copyright (c) 2015 Vadim Ushakov
 * Copyright (c) 2008 LxDE Developers, see the file AUTHORS for details.
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

#define BACKEND_IMPLEMENTATION
#include "backend.h"

#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <alsa/asoundlib.h>
#include <poll.h>

typedef struct {
    volume_control_backend_t * backend;
    snd_mixer_t * mixer;
    snd_mixer_selem_id_t * sid;
    snd_mixer_elem_t * master_element;
    guint mixer_evt_idle;
    guint restart_idle;

    GIOChannel ** channels;
    guint * watches;
    guint num_channels;

    gboolean valid;

    gpointer frontend;
    void (*frontend_callback_state_changed)(gpointer frontend, gpointer backend);
} backend_alsa_t;

static gboolean alsa_find_element(backend_alsa_t * impl, const char * ename);
static gboolean alsa_reset_mixer_evt_idle(backend_alsa_t * impl);
static gboolean alsa_mixer_event(GIOChannel * channel, GIOCondition cond, backend_alsa_t * impl);
static gboolean alsa_restart(gpointer impl_gpointer);
static gboolean alsa_initialize(backend_alsa_t * backend);
static void alsa_deinitialize(backend_alsa_t * backend);
static gboolean alsa_has_mute(volume_control_backend_t * backend);
static gboolean alsa_is_muted(volume_control_backend_t * backend);
static long alsa_get_volume(volume_control_backend_t * backend);
static void alsa_set_volume(volume_control_backend_t * backend, long volume);

#define MY_NAME "volume control: alsa backend"

static gboolean alsa_find_element(backend_alsa_t * impl, const char * ename)
{
    for (
      impl->master_element = snd_mixer_first_elem(impl->mixer);
      impl->master_element != NULL;
      impl->master_element = snd_mixer_elem_next(impl->master_element))
    {
        snd_mixer_selem_get_id(impl->master_element, impl->sid);
        if ((snd_mixer_selem_is_active(impl->master_element))
        && (g_strcmp0(ename, snd_mixer_selem_id_get_name(impl->sid)) == 0))
            return TRUE;
    }
    return FALSE;
}

/* NOTE by PCMan:
 * This is magic! Since ALSA uses its own machanism to handle this part.
 * After polling of mixer fds, it requires that we should call
 * snd_mixer_handle_events to clear all pending mixer events.
 * However, when using the glib IO channels approach, we don't have
 * poll() and snd_mixer_poll_descriptors_revents(). Due to the design of
 * glib, on_mixer_event() will be called for every fd whose status was
 * changed. So, after each poll(), it's called for several times,
 * not just once. Therefore, we cannot call snd_mixer_handle_events()
 * directly in the event handler. Otherwise, it will get called for
 * several times, which might clear unprocessed pending events in the queue.
 * So, here we call it once in the event callback for the first fd.
 * Then, we don't call it for the following fds. After all fds with changed
 * status are handled, we remove this restriction in an idle handler.
 * The next time the event callback is involked for the first fs, we can
 * call snd_mixer_handle_events() again. Racing shouldn't happen here
 * because the idle handler has the same priority as the io channel callback.
 * So, io callbacks for future pending events should be in the next gmain
 * iteration, and won't be affected.
 */

static gboolean alsa_reset_mixer_evt_idle(backend_alsa_t * impl)
{
    if (!g_source_is_destroyed(g_main_current_source()))
        impl->mixer_evt_idle = 0;
    return FALSE;
}

/* Handler for I/O event on ALSA channel. */
static gboolean alsa_mixer_event(GIOChannel * channel, GIOCondition cond, backend_alsa_t * impl)
{
    int res = 0;

    if (impl->mixer_evt_idle == 0)
    {
        impl->mixer_evt_idle = g_idle_add_full(G_PRIORITY_DEFAULT, (GSourceFunc) alsa_reset_mixer_evt_idle, impl, NULL);
        res = snd_mixer_handle_events(impl->mixer);
    }

    if (cond & G_IO_IN)
    {
        volume_control_backend_notify_state_changed(impl->backend);
    }

    if ((cond & G_IO_HUP) || (res < 0))
    {
        /* This means there're some problems with alsa. */
        g_warning(
            MY_NAME ": ALSA (or pulseaudio) had a problem:\n"
            "snd_mixer_handle_events() = %d,  cond 0x%x (IN: 0x%x, HUP: 0x%x).",
            res, cond, G_IO_IN, G_IO_HUP);
        if (impl->restart_idle == 0)
            impl->restart_idle = g_timeout_add_seconds(3, alsa_restart, impl);
        impl->valid = FALSE;
        volume_control_backend_notify_state_changed(impl->backend);
        return FALSE;
    }

    return TRUE;
}

static gboolean alsa_restart(gpointer impl_gpointer)
{
    backend_alsa_t * impl = impl_gpointer;

    if (!impl)
        return FALSE;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    alsa_deinitialize(impl);

    if (!alsa_initialize(impl)) {
        g_warning(MY_NAME ": Re-initialization failed.");
        return TRUE; // try again
    }

    g_warning(MY_NAME ": Restarted ALSA interface...");

    impl->restart_idle = 0;
    return FALSE;
}

/* Initialize the ALSA interface. */
static gboolean alsa_initialize(backend_alsa_t * impl)
{
    impl->valid = FALSE;

    /* Access the "default" device. */
    if (snd_mixer_selem_id_malloc(&impl->sid) != 0)
        return FALSE;
    if (snd_mixer_open(&impl->mixer, 0) != 0)
        return FALSE;
    if (snd_mixer_attach(impl->mixer, "default") != 0)
        return FALSE;
    if (snd_mixer_selem_register(impl->mixer, NULL, NULL) != 0)
        return FALSE;
    if (snd_mixer_load(impl->mixer) != 0)
        return FALSE;

    /* Find Master element, or Front element, or PCM element, or LineOut element.
     * If one of these succeeds, master_element is valid. */
    if (!alsa_find_element(impl, "Master"))
        if (!alsa_find_element(impl, "Front"))
            if (!alsa_find_element(impl, "PCM"))
                if (!alsa_find_element(impl, "LineOut"))
                    return FALSE;

    /* Set the playback volume range as we wish it. */
    snd_mixer_selem_set_playback_volume_range(impl->master_element, 0, 100);

    /* Listen to events from ALSA. */
    int n_fds = snd_mixer_poll_descriptors_count(impl->mixer);
    struct pollfd * fds = g_new0(struct pollfd, n_fds);

    impl->channels = g_new0(GIOChannel *, n_fds);
    impl->watches  = g_new0(guint, n_fds);
    impl->num_channels = n_fds;

    snd_mixer_poll_descriptors(impl->mixer, fds, n_fds);
    int i;
    for (i = 0; i < n_fds; ++i)
    {
        GIOChannel* channel = g_io_channel_unix_new(fds[i].fd);
        impl->watches[i] = g_io_add_watch(channel, G_IO_IN | G_IO_HUP, (GIOFunc) alsa_mixer_event, impl);
        impl->channels[i] = channel;
    }
    g_free(fds);
    impl->valid = TRUE;
    volume_control_backend_notify_state_changed(impl->backend);
    return TRUE;
}

static void alsa_deinitialize(backend_alsa_t * impl)
{
    if (!impl)
        return;

    if (impl->mixer_evt_idle != 0) {
        g_source_remove(impl->mixer_evt_idle);
        impl->mixer_evt_idle = 0;
    }

    guint i;
    for (i = 0; i < impl->num_channels; i++) {
        g_source_remove(impl->watches[i]);
        g_io_channel_shutdown(impl->channels[i], FALSE, NULL);
        g_io_channel_unref(impl->channels[i]);
    }
    g_free(impl->channels);
    g_free(impl->watches);
    impl->channels = NULL;
    impl->watches = NULL;
    impl->num_channels = 0;

    snd_mixer_close(impl->mixer);
    impl->master_element = NULL;

    snd_mixer_selem_id_free(impl->sid);
}

/* Get the presence of the mute control from the sound system. */
static gboolean alsa_has_mute(volume_control_backend_t * backend)
{
    if (!backend || !backend->impl)
        return FALSE;

    backend_alsa_t * impl = backend->impl;

    return ((impl->master_element != NULL) ? snd_mixer_selem_has_playback_switch(impl->master_element) : FALSE);
}

/* Get the condition of the mute control from the sound system. */
static gboolean alsa_is_muted(volume_control_backend_t * backend)
{
    if (!backend || !backend->impl)
        return FALSE;

    backend_alsa_t * impl = backend->impl;

    /* The switch is on if sound is not muted, and off if the sound is muted.
     * Initialize so that the sound appears unmuted if the control does not exist. */
    int value = 1;
    if (impl->master_element != NULL)
        snd_mixer_selem_get_playback_switch(impl->master_element, 0, &value);
    return (value == 0);
}

/* Get the volume from the sound system.
 * This implementation returns the average of the Front Left and Front Right channels. */
static long alsa_get_volume(volume_control_backend_t * backend)
{
    if (!backend || !backend->impl)
        return 0;

    backend_alsa_t * impl = backend->impl;

    long aleft = 0;
    long aright = 0;
    if (impl->master_element != NULL)
    {
        snd_mixer_selem_get_playback_volume(impl->master_element, SND_MIXER_SCHN_FRONT_LEFT, &aleft);
        snd_mixer_selem_get_playback_volume(impl->master_element, SND_MIXER_SCHN_FRONT_RIGHT, &aright);
    }
    return (aleft + aright) >> 1;
}

/* Set the volume to the sound system.
 * This implementation sets the Front Left and Front Right channels to the specified value. */
static void alsa_set_volume(volume_control_backend_t * backend, long volume)
{
    if (!backend || !backend->impl)
        return;

    backend_alsa_t * impl = backend->impl;

    if (alsa_get_volume(backend) == volume)
        return;

    if (impl->master_element != NULL)
    {
        /*snd_mixer_selem_set_playback_volume(impl->master_element, SND_MIXER_SCHN_FRONT_LEFT, volume);
        snd_mixer_selem_set_playback_volume(impl->master_element, SND_MIXER_SCHN_FRONT_RIGHT, volume);*/
        snd_mixer_selem_set_playback_volume_all(impl->master_element, volume);
    }
    volume_control_backend_notify_state_changed(backend);
}

static void alsa_set_mute(volume_control_backend_t * backend, gboolean value)
{
    if (!backend || !backend->impl)
        return;

    backend_alsa_t * impl = backend->impl;

    /* Reflect the mute toggle to the sound system. */
    if (impl->master_element != NULL)
    {
        int chn;
        for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++)
            snd_mixer_selem_set_playback_switch(impl->master_element, chn, (value) ? 0 : 1);
    }
    volume_control_backend_notify_state_changed(backend);
}

static gboolean alsa_is_valid(volume_control_backend_t * backend)
{
    if (!backend || !backend->impl)
        return FALSE;

    backend_alsa_t * impl = backend->impl;
    return impl->valid;
}

static void alsa_destroy(volume_control_backend_t * backend)
{
    if (!backend || !backend->impl)
        return;

    backend_alsa_t * impl = backend->impl;

    alsa_deinitialize(impl);

    if (impl->restart_idle != 0) {
        g_source_remove(impl->restart_idle);
        impl->restart_idle = 0;
    }

    g_free(impl);
    backend->impl = NULL;
    backend->impl_vtable = NULL;
}

static volume_control_backend_impl_vtable_t alsa_vtable = 
{
    .is_valid =alsa_is_valid,
    .has_mute = alsa_has_mute,
    .is_muted = alsa_is_muted,
    .set_mute = alsa_set_mute,
    .get_volume = alsa_get_volume,
    .set_volume = alsa_set_volume,
    .destroy = alsa_destroy
};

void volume_control_backend_alsa_new(volume_control_backend_t * backend)
{
    if (!backend)
        return;

    backend->impl_vtable = &alsa_vtable;
    backend_alsa_t * impl = g_new0(backend_alsa_t, 1);
    backend->impl = impl;
    impl->backend = backend;
    alsa_initialize(impl);
}
