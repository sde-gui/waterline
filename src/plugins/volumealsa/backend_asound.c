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

#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <alsa/asoundlib.h>
#include <poll.h>

typedef struct {
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

static gboolean asound_find_element(backend_alsa_t * backend, const char * ename);
static gboolean asound_reset_mixer_evt_idle(backend_alsa_t * backend);
static gboolean asound_mixer_event(GIOChannel * channel, GIOCondition cond, backend_alsa_t * backend);
static gboolean asound_restart(gpointer backend_gpointer);
static gboolean asound_initialize(backend_alsa_t * backend);
static void asound_deinitialize(backend_alsa_t * backend);
static gboolean asound_has_mute(backend_alsa_t * backend);
static gboolean asound_is_muted(backend_alsa_t * backend);
static long asound_get_volume(backend_alsa_t * backend);
static void asound_set_volume(backend_alsa_t * backend, long volume);

#define MY_NAME "volume control: alsa backend"

static void asound_notify_state_changed(backend_alsa_t * backend)
{
    if (backend->frontend_callback_state_changed)
        (backend->frontend_callback_state_changed)(backend->frontend, backend);
}

static gboolean asound_find_element(backend_alsa_t * backend, const char * ename)
{
    for (
      backend->master_element = snd_mixer_first_elem(backend->mixer);
      backend->master_element != NULL;
      backend->master_element = snd_mixer_elem_next(backend->master_element))
    {
        snd_mixer_selem_get_id(backend->master_element, backend->sid);
        if ((snd_mixer_selem_is_active(backend->master_element))
        && (g_strcmp0(ename, snd_mixer_selem_id_get_name(backend->sid)) == 0))
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

static gboolean asound_reset_mixer_evt_idle(backend_alsa_t * backend)
{
    if (!g_source_is_destroyed(g_main_current_source()))
        backend->mixer_evt_idle = 0;
    return FALSE;
}

/* Handler for I/O event on ALSA channel. */
static gboolean asound_mixer_event(GIOChannel * channel, GIOCondition cond, backend_alsa_t * backend)
{
    int res = 0;

    if (backend->mixer_evt_idle == 0)
    {
        backend->mixer_evt_idle = g_idle_add_full(G_PRIORITY_DEFAULT, (GSourceFunc) asound_reset_mixer_evt_idle, backend, NULL);
        res = snd_mixer_handle_events(backend->mixer);
    }

    if (cond & G_IO_IN)
    {
        asound_notify_state_changed(backend);
    }

    if ((cond & G_IO_HUP) || (res < 0))
    {
        /* This means there're some problems with alsa. */
        g_warning(
            MY_NAME ": ALSA (or pulseaudio) had a problem:\n"
            "snd_mixer_handle_events() = %d,  cond 0x%x (IN: 0x%x, HUP: 0x%x).",
            res, cond, G_IO_IN, G_IO_HUP);
        if (backend->restart_idle == 0)
            backend->restart_idle = g_timeout_add_seconds(3, asound_restart, backend);
        backend->valid = FALSE;
        asound_notify_state_changed(backend);
        return FALSE;
    }

    return TRUE;
}

static gboolean asound_restart(gpointer backend_gpointer)
{
    backend_alsa_t * backend = backend_gpointer;

    if (!backend)
        return FALSE;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    asound_deinitialize(backend);

    if (!asound_initialize(backend)) {
        g_warning(MY_NAME ": Re-initialization failed.");
        return TRUE; // try again
    }

    g_warning(MY_NAME ": Restarted ALSA interface...");

    backend->restart_idle = 0;
    return FALSE;
}

/* Initialize the ALSA interface. */
static gboolean asound_initialize(backend_alsa_t * backend)
{
    if (!backend)
        return FALSE;

    backend->valid = FALSE;

    /* Access the "default" device. */
    if (snd_mixer_selem_id_malloc(&backend->sid) != 0)
        return FALSE;
    if (snd_mixer_open(&backend->mixer, 0) != 0)
        return FALSE;
    if (snd_mixer_attach(backend->mixer, "default") != 0)
        return FALSE;
    if (snd_mixer_selem_register(backend->mixer, NULL, NULL) != 0)
        return FALSE;
    if (snd_mixer_load(backend->mixer) != 0)
        return FALSE;

    /* Find Master element, or Front element, or PCM element, or LineOut element.
     * If one of these succeeds, master_element is valid. */
    if (!asound_find_element(backend, "Master"))
        if (!asound_find_element(backend, "Front"))
            if (!asound_find_element(backend, "PCM"))
                if (!asound_find_element(backend, "LineOut"))
                    return FALSE;

    /* Set the playback volume range as we wish it. */
    snd_mixer_selem_set_playback_volume_range(backend->master_element, 0, 100);

    /* Listen to events from ALSA. */
    int n_fds = snd_mixer_poll_descriptors_count(backend->mixer);
    struct pollfd * fds = g_new0(struct pollfd, n_fds);

    backend->channels = g_new0(GIOChannel *, n_fds);
    backend->watches  = g_new0(guint, n_fds);
    backend->num_channels = n_fds;

    snd_mixer_poll_descriptors(backend->mixer, fds, n_fds);
    int i;
    for (i = 0; i < n_fds; ++i)
    {
        GIOChannel* channel = g_io_channel_unix_new(fds[i].fd);
        backend->watches[i] = g_io_add_watch(channel, G_IO_IN | G_IO_HUP, (GIOFunc) asound_mixer_event, backend);
        backend->channels[i] = channel;
    }
    g_free(fds);
    backend->valid = TRUE;
    asound_notify_state_changed(backend);
    return TRUE;
}

static void asound_deinitialize(backend_alsa_t * backend)
{
    if (!backend)
        return;

    if (backend->mixer_evt_idle != 0) {
        g_source_remove(backend->mixer_evt_idle);
        backend->mixer_evt_idle = 0;
    }

    guint i;
    for (i = 0; i < backend->num_channels; i++) {
        g_source_remove(backend->watches[i]);
        g_io_channel_shutdown(backend->channels[i], FALSE, NULL);
        g_io_channel_unref(backend->channels[i]);
    }
    g_free(backend->channels);
    g_free(backend->watches);
    backend->channels = NULL;
    backend->watches = NULL;
    backend->num_channels = 0;

    snd_mixer_close(backend->mixer);
    backend->master_element = NULL;

    snd_mixer_selem_id_free(backend->sid);
}

/* Get the presence of the mute control from the sound system. */
static gboolean asound_has_mute(backend_alsa_t * backend)
{
    if (!backend)
        return FALSE;

    return ((backend->master_element != NULL) ? snd_mixer_selem_has_playback_switch(backend->master_element) : FALSE);
}

/* Get the condition of the mute control from the sound system. */
static gboolean asound_is_muted(backend_alsa_t * backend)
{
    if (!backend)
        return FALSE;

    /* The switch is on if sound is not muted, and off if the sound is muted.
     * Initialize so that the sound appears unmuted if the control does not exist. */
    int value = 1;
    if (backend->master_element != NULL)
        snd_mixer_selem_get_playback_switch(backend->master_element, 0, &value);
    return (value == 0);
}

/* Get the volume from the sound system.
 * This implementation returns the average of the Front Left and Front Right channels. */
static long asound_get_volume(backend_alsa_t * backend)
{
    if (!backend)
        return 0;

    long aleft = 0;
    long aright = 0;
    if (backend->master_element != NULL)
    {
        snd_mixer_selem_get_playback_volume(backend->master_element, SND_MIXER_SCHN_FRONT_LEFT, &aleft);
        snd_mixer_selem_get_playback_volume(backend->master_element, SND_MIXER_SCHN_FRONT_RIGHT, &aright);
    }
    return (aleft + aright) >> 1;
}

/* Set the volume to the sound system.
 * This implementation sets the Front Left and Front Right channels to the specified value. */
static void asound_set_volume(backend_alsa_t * backend, long volume)
{
    if (!backend)
        return;

    if (asound_get_volume(backend) == volume)
        return;

    if (backend->master_element != NULL)
    {
        /*snd_mixer_selem_set_playback_volume(backend->master_element, SND_MIXER_SCHN_FRONT_LEFT, volume);
        snd_mixer_selem_set_playback_volume(backend->master_element, SND_MIXER_SCHN_FRONT_RIGHT, volume);*/
        snd_mixer_selem_set_playback_volume_all(backend->master_element, volume);
    }
    asound_notify_state_changed(backend);
}

static void asound_set_mute(backend_alsa_t * backend, gboolean value)
{
    if (!backend)
        return;

    /* Reflect the mute toggle to the sound system. */
    if (backend->master_element != NULL)
    {
        int chn;
        for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++)
            snd_mixer_selem_set_playback_switch(backend->master_element, chn, (value) ? 0 : 1);
    }
    asound_notify_state_changed(backend);
}

static gboolean asound_is_valid(backend_alsa_t * backend)
{
    return backend && backend->valid;
}


static void asound_destroy(backend_alsa_t * backend)
{
    if (!backend)
        return;

    asound_deinitialize(backend);

    if (backend->restart_idle != 0) {
        g_source_remove(backend->restart_idle);
        backend->restart_idle != 0;
    }
}
