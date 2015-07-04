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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <sde-utils-jansson.h>

#define PLUGIN_PRIV_TYPE VolumeALSAPlugin

#include <waterline/defaultapplications.h>
#include <waterline/panel.h>
#include <waterline/gtkcompat.h>
#include <waterline/misc.h>
#include <waterline/launch.h>
#include <waterline/paths.h>
#include <waterline/plugin.h>

#include <sde-utils.h>

#include "backend.h"

typedef struct {

    /* Graphics. */
    Plugin * plugin;                /* Back pointer to plugin */
    GtkWidget * tray_icon;          /* Displayed image */
    GtkWidget * popup_window;       /* Top level window for popup */
    GtkWidget * volume_scale;       /* Scale for volume */
    GtkWidget * mute_check;         /* Checkbox for mute state */
    gboolean show_popup;            /* Toggle to show and hide the popup on left click */
    guint volume_scale_handler;     /* Handler for vscale widget */
    guint mute_check_handler;       /* Handler for mute_check widget */

    guint theme_changed_handler_id;

    GdkPixbuf * pixbuf_mute;
    GdkPixbuf * pixbuf_level_0;
    GdkPixbuf * pixbuf_level_33;
    GdkPixbuf * pixbuf_level_66;
    GdkPixbuf * pixbuf_level_100;

    GHashTable * pixbuf_blend_cache;

    gboolean displayed_valid;
    long displayed_scaled_volume;
    gboolean displayed_mute;
    gboolean displayed_has_mute;

    /* State. */
    gboolean valid;
    long scaled_volume;
    gboolean mute;
    gboolean has_mute;

    /* Settings. */
    gchar * volume_control_command;
    gboolean alpha_blending_enabled;

    /* Backend. */
    volume_control_backend_t * backend;
} VolumeALSAPlugin;

static void volumealsa_update_display(VolumeALSAPlugin * vol, gboolean force);
static void volumealsa_state_changed(VolumeALSAPlugin * vol, gpointer backend);
static gboolean volumealsa_button_press_event(GtkWidget * widget, GdkEventButton * event, VolumeALSAPlugin * vol);
static gboolean volumealsa_popup_focus_out(GtkWidget * widget, GdkEvent * event, VolumeALSAPlugin * vol);
static void volumealsa_popup_map(GtkWidget * widget, VolumeALSAPlugin * vol);
static void volumealsa_popup_scale_changed(GtkRange * range, VolumeALSAPlugin * vol);
static void volumealsa_popup_scale_scrolled(GtkScale * scale, GdkEventScroll * evt, VolumeALSAPlugin * vol);
static void volumealsa_popup_mute_toggled(GtkWidget * widget, VolumeALSAPlugin * vol);
static void volumealsa_build_popup_window(VolumeALSAPlugin * vol);
static int volumealsa_constructor(Plugin * p);
static void volumealsa_destructor(Plugin * p);
static void volumealsa_panel_configuration_changed(Plugin * p);

/******************************************************************************/

#define SU_JSON_OPTION_STRUCTURE VolumeALSAPlugin
static su_json_option_definition option_definitions[] = {
    SU_JSON_OPTION(string, volume_control_command),
    SU_JSON_OPTION(bool, alpha_blending_enabled),
    {0,}
};

/******************************************************************************/

static void volumealsa_show_popup(VolumeALSAPlugin * vol)
{
    if (!vol->displayed_valid)
        return;
    if (vol->show_popup)
        return;

    volumealsa_build_popup_window(vol);

    int optimal_height = 140;
    int available_screen_height = panel_get_available_screen_height(plugin_panel(vol->plugin));
    if (available_screen_height > optimal_height)
    {
        optimal_height += (available_screen_height - optimal_height) / 10;
    }
    gtk_window_set_default_size(GTK_WINDOW(vol->popup_window), 80, optimal_height);

    plugin_adjust_popup_position(vol->popup_window, vol->plugin);
    gtk_widget_show_all(vol->popup_window);
    vol->show_popup = TRUE;
}

static void volumealsa_hide_popup(VolumeALSAPlugin * vol)
{
    if (!vol->show_popup)
        return;

    gtk_widget_hide(vol->popup_window);
    vol->show_popup = FALSE;
}

/* Handler for "button-press-event" signal on main widget. */
static const char * volumealsa_get_volume_control_command(VolumeALSAPlugin * vol)
{
    if (su_str_empty(vol->volume_control_command))
        return wtl_get_default_application("volume-control");
    else
        return vol->volume_control_command;
}

static GdkPixbuf * pixbuf_blend(GdkPixbuf * pixbuf_src1, GdkPixbuf * pixbuf_src2, float level)
{
    if (gdk_pixbuf_get_colorspace(pixbuf_src1) != GDK_COLORSPACE_RGB)
        return NULL;
    if (gdk_pixbuf_get_bits_per_sample(pixbuf_src1) != 8)
        return NULL;

    if (gdk_pixbuf_get_colorspace(pixbuf_src2) != GDK_COLORSPACE_RGB)
        return NULL;
    if (gdk_pixbuf_get_bits_per_sample(pixbuf_src2) != 8)
        return NULL;

    int src1_n_channels = gdk_pixbuf_get_n_channels(pixbuf_src1);
    gboolean src1_has_alpha = gdk_pixbuf_get_has_alpha(pixbuf_src1);
    int src2_n_channels = gdk_pixbuf_get_n_channels(pixbuf_src2);
    gboolean src2_has_alpha = gdk_pixbuf_get_has_alpha(pixbuf_src2);

    int w = MIN(gdk_pixbuf_get_width(pixbuf_src1), gdk_pixbuf_get_width(pixbuf_src2));
    int h = MIN(gdk_pixbuf_get_height(pixbuf_src1), gdk_pixbuf_get_height(pixbuf_src2));

    GdkPixbuf * pixbuf_dst = gdk_pixbuf_new(
        GDK_COLORSPACE_RGB,
        src1_has_alpha || src2_has_alpha,
        8,
        w, h);

    if (!pixbuf_dst)
        return NULL;

    int dst_n_channels = gdk_pixbuf_get_n_channels(pixbuf_dst);

    guchar * dst = gdk_pixbuf_get_pixels(pixbuf_dst);
    guchar * src1 = gdk_pixbuf_get_pixels(pixbuf_src1);
    guchar * src2 = gdk_pixbuf_get_pixels(pixbuf_src2);

    int dst_stride = gdk_pixbuf_get_rowstride(pixbuf_dst);
    int src1_stride = gdk_pixbuf_get_rowstride(pixbuf_src1);
    int src2_stride = gdk_pixbuf_get_rowstride(pixbuf_src2);

    int i;
    for (i = 0; i < h; i += 1)
    {
        int j;
        for (j = 0; j < w; j += 1)
        {
            guchar * s1 = src1 + i * src1_stride + j * 4;
            guchar * s2 = src2 + i * src2_stride + j * 4;
            guchar * d = dst + i * dst_stride + j * 4;
            int n;
            for (n = 0; n < dst_n_channels; n++) {
                guchar v1 = (n < src1_n_channels) ? s1[n] : 0xFF;
                guchar v2 = (n < src2_n_channels) ? s2[n] : 0xFF;
                d[n] = v1 * (1 - level) + v2 * (level);
            }
        }
    }

    return pixbuf_dst;
}

static void load_icon(GdkPixbuf ** p_pixbuf, int icon_size, ...)
{
    if (*p_pixbuf) {
        g_object_unref(*p_pixbuf);
        *p_pixbuf = NULL;
    }

    va_list ap;
    va_start(ap, icon_size);
    while (!*p_pixbuf) {
        const char * name = va_arg(ap, const char *);
        if (!name)
            break;
        *p_pixbuf = wtl_load_icon(name, icon_size, icon_size, FALSE);
    }
    va_end(ap);
}


static void volumealsa_load_icons(VolumeALSAPlugin * vol)
{
    int icon_size = plugin_get_icon_size(vol->plugin);

    load_icon(&vol->pixbuf_mute, icon_size,
        "audio-volume-muted-panel",
        "audio-volume-muted",
        "xfce4-mixer-muted",
        "stock_volume-mute",
        NULL);

    load_icon(&vol->pixbuf_level_0, icon_size,
        "audio-volume-zero-panel",
        "audio-volume-off",
        "audio-volume-low-panel",
        "audio-volume-low",
        "xfce4-mixer-volume-ultra-low",
        "stock_volume-min",
        NULL);

    load_icon(&vol->pixbuf_level_33, icon_size,
        "audio-volume-low-panel",
        "audio-volume-low",
        "xfce4-mixer-volume-low",
        "stock_volume-min",
        NULL);

    load_icon(&vol->pixbuf_level_66, icon_size,
        "audio-volume-medium-panel",
        "audio-volume-medium",
        "xfce4-mixer-volume-medium",
        "stock_volume-med",
        NULL);

    load_icon(&vol->pixbuf_level_100, icon_size,
        "audio-volume-high-panel",
        "audio-volume-high",
        "xfce4-mixer-volume-high",
        "stock_volume-max",
        NULL);

    if (!vol->pixbuf_mute) {
        gchar * icon_path = wtl_resolve_own_resource("", "images", "mute.png", 0);
        vol->pixbuf_mute = wtl_load_icon(icon_path, icon_size, icon_size, TRUE);
        g_free(icon_path);
    }

    if (!vol->pixbuf_level_0 || !vol->pixbuf_level_33 || !vol->pixbuf_level_66 || !vol->pixbuf_level_100) {
        gchar * icon_path = wtl_resolve_own_resource("", "images", "volume.png", 0);
        GdkPixbuf * pixbuf = wtl_load_icon(icon_path, icon_size, icon_size, TRUE);
        if (!vol->pixbuf_level_0)
            vol->pixbuf_level_0 = g_object_ref(pixbuf);
        if (!vol->pixbuf_level_33)
            vol->pixbuf_level_33 = g_object_ref(pixbuf);
        if (!vol->pixbuf_level_66)
            vol->pixbuf_level_66 = g_object_ref(pixbuf);
        if (!vol->pixbuf_level_100)
            vol->pixbuf_level_100 = g_object_ref(pixbuf);
        g_object_unref(pixbuf);
        g_free(icon_path);
    }

    if (vol->pixbuf_blend_cache) {
        g_hash_table_unref(vol->pixbuf_blend_cache);
        vol->pixbuf_blend_cache = NULL;
    }
}

static GdkPixbuf * volumealsa_get_icon_for_level(VolumeALSAPlugin * vol, int level)
{
    GdkPixbuf * pixbuf_level_low = NULL;
    GdkPixbuf * pixbuf_level_high = NULL;
    float l;

    if (level < 33)
    {
        pixbuf_level_low = vol->pixbuf_level_0;
        pixbuf_level_high = vol->pixbuf_level_33;
        l = level / 33.0;
    }
    else if (level < 66)
    {
        pixbuf_level_low = vol->pixbuf_level_33;
        pixbuf_level_high = vol->pixbuf_level_66;
        l = (level - 33) / 33.0;
    }
    else
    {
        pixbuf_level_low = vol->pixbuf_level_66;
        pixbuf_level_high = vol->pixbuf_level_100;
        l = (level - 66) / 34.0;
    }

    if (!vol->alpha_blending_enabled) {
        if (level == 0)
            return g_object_ref(pixbuf_level_low);
        else
            return g_object_ref(pixbuf_level_high);
    }

    if (!vol->pixbuf_blend_cache)
    {
        vol->pixbuf_blend_cache = g_hash_table_new_full(
            g_direct_hash,
            g_direct_equal,
            NULL,
            g_object_unref
        );
    }
    else
    {
        GdkPixbuf * result = g_hash_table_lookup(vol->pixbuf_blend_cache, GINT_TO_POINTER(level));
        if (result)
            return g_object_ref(result);
    }

    if (!pixbuf_level_low || !pixbuf_level_high)
        return NULL;

    if (l > 1.0)
        l = 1.0;

    GdkPixbuf * result = pixbuf_blend(pixbuf_level_low, pixbuf_level_high, l);
    if (!result)
        result = g_object_ref(pixbuf_level_high);

    g_hash_table_replace(vol->pixbuf_blend_cache, GINT_TO_POINTER(level), result);
    return g_object_ref(result);
}

static void volumealsa_update_icon(VolumeALSAPlugin * vol)
{
    if (vol->displayed_mute || !vol->displayed_valid)
    {
        gtk_image_set_from_pixbuf(GTK_IMAGE(vol->tray_icon), vol->pixbuf_mute);
    }
    else
    {
        GdkPixbuf * pixbuf_level = volumealsa_get_icon_for_level(vol, vol->displayed_scaled_volume);
        gtk_image_set_from_pixbuf(GTK_IMAGE(vol->tray_icon), pixbuf_level);
        g_object_unref(pixbuf_level);
    }
}

static void on_theme_changed(GtkIconTheme * theme, VolumeALSAPlugin * vol)
{
    volumealsa_load_icons(vol);
    volumealsa_update_display(vol, TRUE);
}

static void volumealsa_update_display(VolumeALSAPlugin * vol, gboolean force)
{
    if (!force &&
        vol->displayed_valid == vol->valid && 
        vol->displayed_scaled_volume == vol->scaled_volume &&
        vol->displayed_mute == vol->mute && 
        vol->displayed_has_mute == vol->has_mute)
        return;

    vol->displayed_valid = vol->valid;
    vol->displayed_scaled_volume = vol->scaled_volume;
    vol->displayed_mute = vol->mute;
    vol->displayed_has_mute = vol->has_mute;

    volumealsa_update_icon(vol);

    if (vol->displayed_valid)
    {
        if (vol->volume_scale != NULL)
        {
            g_signal_handler_block(vol->mute_check, vol->mute_check_handler);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vol->mute_check), vol->displayed_mute);
            gtk_widget_set_sensitive(vol->mute_check, vol->displayed_has_mute);
            g_signal_handler_unblock(vol->mute_check, vol->mute_check_handler);
        }

        if (vol->volume_scale != NULL)
        {
            g_signal_handler_block(vol->volume_scale, vol->volume_scale_handler);
            gtk_range_set_value(GTK_RANGE(vol->volume_scale), vol->displayed_scaled_volume);
            g_signal_handler_unblock(vol->volume_scale, vol->volume_scale_handler);
        }
    }
    else
    {
        volumealsa_hide_popup(vol);
    }

    char * tooltip = NULL;
    if (!vol->displayed_valid)
        tooltip = g_strdup_printf(_("<i>An internal error occured.\nVolume Control is not functioning properly.</i>"));
    else if (vol->displayed_mute)
        tooltip = g_strdup_printf(_("Volume <b>%ld%%</b> (muted)"), vol->displayed_scaled_volume);
    else
        tooltip = g_strdup_printf(_("Volume <b>%ld%%</b>"), vol->displayed_scaled_volume);
    gtk_widget_set_tooltip_markup(plugin_widget(vol->plugin), tooltip);
    g_free(tooltip);
}

static void volumealsa_state_changed(VolumeALSAPlugin * vol, gpointer _backend)
{
    vol->valid = volume_control_backend_is_valid(vol->backend);
    vol->has_mute = volume_control_backend_has_mute(vol->backend);
    vol->mute = volume_control_backend_is_muted(vol->backend);
    vol->scaled_volume = volume_control_backend_get_volume(vol->backend);
    volumealsa_update_display(vol, FALSE);
}

/* Handler for "button-press-event" signal on main widget. */
static gboolean volumealsa_button_press_event(GtkWidget * widget, GdkEventButton * event, VolumeALSAPlugin * vol)
{
    /* Standard right-click handling. */
    if (plugin_button_press_event(widget, event, vol->plugin))
        return TRUE;

    /* Left-click.  Show or hide the popup window. */
    if (event->button == 1)
    {
        if (event->type==GDK_2BUTTON_PRESS)
        {
            volumealsa_hide_popup(vol);
            wtl_launch(volumealsa_get_volume_control_command(vol), NULL);
        }
        else
        {
            if (vol->show_popup)
            {
                volumealsa_hide_popup(vol);
            }
            else if (vol->displayed_valid)
            {
                volumealsa_show_popup(vol);
            }
        }
    }
    /* Middle-click.  Toggle the mute status. */
    else if (event->button == 2)
    {
        volumealsa_build_popup_window(vol);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vol->mute_check), ! gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vol->mute_check)));
    }
    return TRUE;
}

/* Handler for "focus-out" signal on popup window. */
static gboolean volumealsa_popup_focus_out(GtkWidget * widget, GdkEvent * event, VolumeALSAPlugin * vol)
{
    /* Hide the widget. */
    volumealsa_hide_popup(vol);
    return FALSE;
}

/* Handler for "map" signal on popup window. */
static void volumealsa_popup_map(GtkWidget * widget, VolumeALSAPlugin * vol)
{
    plugin_adjust_popup_position(widget, vol->plugin);
}

/* Handler for "value_changed" signal on popup window vertical scale. */
static void volumealsa_popup_scale_changed(GtkRange * range, VolumeALSAPlugin * vol)
{
    volume_control_backend_set_volume(vol->backend, gtk_range_get_value(range));
}

/* Handler for "scroll-event" signal on popup window vertical scale. */
static void volumealsa_popup_scale_scrolled(GtkScale * scale, GdkEventScroll * evt, VolumeALSAPlugin * vol)
{
    volumealsa_build_popup_window(vol);

    /* Get the state of the vertical scale. */
    gdouble val = gtk_range_get_value(GTK_RANGE(vol->volume_scale));

    /* Dispatch on scroll direction to update the value. */
    if ((evt->direction == GDK_SCROLL_UP) || (evt->direction == GDK_SCROLL_LEFT))
        val += 2;
    else
        val -= 2;

    /* Reset the state of the vertical scale.  This provokes a "value_changed" event. */
    gtk_range_set_value(GTK_RANGE(vol->volume_scale), CLAMP((int)val, 0, 100));
}

/* Handler for "toggled" signal on popup window mute checkbox. */
static void volumealsa_popup_mute_toggled(GtkWidget * widget, VolumeALSAPlugin * vol)
{
    /* Get the state of the mute toggle. */
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    volume_control_backend_set_mute(vol->backend, active);
}

/* Build the window that appears when the top level widget is clicked. */
static void volumealsa_build_popup_window(VolumeALSAPlugin * vol)
{
    if (vol->popup_window)
        return;

    /* Create a new window. */
    vol->popup_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(vol->popup_window), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(vol->popup_window), 0);
    gtk_window_set_default_size(GTK_WINDOW(vol->popup_window), 80, 140);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(vol->popup_window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(vol->popup_window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(vol->popup_window), GDK_WINDOW_TYPE_HINT_DIALOG);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(vol->popup_window), "focus_out_event", G_CALLBACK(volumealsa_popup_focus_out), vol);
    g_signal_connect(G_OBJECT(vol->popup_window), "map", G_CALLBACK(volumealsa_popup_map), vol);

    /* Create a viewport as the child of the scrolled window. */
    GtkWidget * viewport = gtk_viewport_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(vol->popup_window), viewport);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_OUT);
    gtk_widget_show(viewport);

    /* Create a frame as the child of the viewport. */
    GtkWidget * frame = gtk_frame_new(_("Volume"));
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
    gtk_container_add(GTK_CONTAINER(viewport), frame);

    /* Create a vertical box as the child of the frame. */
    GtkWidget * box = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), box);

    /* Create a vertical scale as the child of the vertical box. */
    vol->volume_scale = gtk_vscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(100, 0, 100, 0, 0, 0)));
    gtk_scale_set_draw_value(GTK_SCALE(vol->volume_scale), FALSE);
    gtk_range_set_inverted(GTK_RANGE(vol->volume_scale), TRUE);
    gtk_box_pack_start(GTK_BOX(box), vol->volume_scale, TRUE, TRUE, 0);

    /* Value-changed and scroll-event signals. */
    vol->volume_scale_handler = g_signal_connect(vol->volume_scale, "value_changed", G_CALLBACK(volumealsa_popup_scale_changed), vol);
    g_signal_connect(vol->volume_scale, "scroll-event", G_CALLBACK(volumealsa_popup_scale_scrolled), vol);

    /* Create a check button as the child of the vertical box. */
    vol->mute_check = gtk_check_button_new_with_label(_("Mute"));
    gtk_box_pack_end(GTK_BOX(box), vol->mute_check, FALSE, FALSE, 0);
    vol->mute_check_handler = g_signal_connect(vol->mute_check, "toggled", G_CALLBACK(volumealsa_popup_mute_toggled), vol);

    /* Set background to default. */
    gtk_widget_set_style(viewport, panel_get_default_style(plugin_panel(vol->plugin)));

    volumealsa_update_display(vol, TRUE);
}

/* Plugin constructor. */
static int volumealsa_constructor(Plugin * p)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    VolumeALSAPlugin * vol = g_new0(VolumeALSAPlugin, 1);
    vol->plugin = p;
    plugin_set_priv(p, vol);

    vol->volume_control_command = NULL;
    vol->alpha_blending_enabled = TRUE;

    su_json_read_options(plugin_inner_json(p), option_definitions, vol);

    /* Allocate top level widget and set into Plugin widget pointer. */
    GtkWidget * pwid = gtk_event_box_new();
    plugin_set_widget(p, pwid);
    gtk_widget_set_has_window(pwid, FALSE);
    gtk_widget_add_events(pwid, GDK_BUTTON_PRESS_MASK);
    gtk_widget_set_tooltip_text(pwid, _("Volume control"));

    /* Allocate icon as a child of top level. */
    vol->tray_icon = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(pwid), vol->tray_icon);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(pwid), "button-press-event", G_CALLBACK(volumealsa_button_press_event), vol);
    g_signal_connect(G_OBJECT(pwid), "scroll-event", G_CALLBACK(volumealsa_popup_scale_scrolled), vol);
    vol->theme_changed_handler_id =
        g_signal_connect(gtk_icon_theme_get_default(), "changed", G_CALLBACK(on_theme_changed), vol);

    volumealsa_load_icons(vol);

    extern void volume_control_backend_alsa_new(volume_control_backend_t * backend);
    vol->backend = volume_control_backend_new();
    vol->backend->frontend = vol;
    vol->backend->frontend_callback_state_changed = (frontend_callback_state_changed_t) volumealsa_state_changed;
    volume_control_backend_alsa_new(vol->backend);

    volumealsa_update_display(vol, TRUE);
    gtk_widget_show_all(pwid);
    return 1;
}

/* Plugin destructor. */
static void volumealsa_destructor(Plugin * p)
{
    VolumeALSAPlugin * vol = PRIV(p);

    g_signal_handler_disconnect(gtk_icon_theme_get_default(), vol->theme_changed_handler_id);

    volume_control_backend_free(vol->backend);
    vol->backend = NULL;

    /* If the dialog box is open, dismiss it. */
    if (vol->popup_window != NULL) {
        gtk_widget_destroy(vol->popup_window);
        vol->popup_window = NULL;
    }

    g_free(vol->volume_control_command);

    /* Deallocate all memory. */
    g_free(vol);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static void volumealsa_apply_configuration(Plugin * p)
{
    volumealsa_panel_configuration_changed(p);
}

/* Callback when the configuration dialog is to be shown. */
static void volumealsa_configure(Plugin * p, GtkWindow * parent)
{
    VolumeALSAPlugin * vol = PRIV(p);

    const char * volume_control_application = wtl_get_default_application("volume-control");
    gchar * tooltip = NULL;
    if (volume_control_application)
        tooltip = g_strdup_printf(_("Application to run by double click. \"%s\" by default."), volume_control_application);
    else
        tooltip = g_strdup(_("Application to run by double click"));

    GtkWidget * dialog = wtl_create_generic_config_dialog(
        _(plugin_class(p)->name),
        GTK_WIDGET(parent),
        (GSourceFunc) volumealsa_apply_configuration, (gpointer) p,
        _("Volume Control application"), &vol->volume_control_command, (GType)CONF_TYPE_STR,
        "tooltip-text", tooltip, (GType)CONF_TYPE_SET_PROPERTY,

        _("Alpha-blending"), &vol->alpha_blending_enabled, (GType)CONF_TYPE_BOOL,
        "tooltip-text", _("An icon theme typically includes only 3 or 4 distinct icons intended to indicate the sound volume. This option enables alpha-blending mode that allows Volume Control Plugin to smoothly display the full range of the sound volume. May not interact well with some icon themes."), (GType)CONF_TYPE_SET_PROPERTY,

        NULL);

    g_free(tooltip);

    if (dialog)
        gtk_window_present(GTK_WINDOW(dialog));
}

/* Save the configuration to the configuration file. */
static void volumealsa_save_configuration(Plugin * p)
{
    VolumeALSAPlugin * vol = PRIV(p);
    su_json_write_options(plugin_inner_json(p), option_definitions, vol);
}


/* Callback when panel configuration changes. */
static void volumealsa_panel_configuration_changed(Plugin * p)
{
    volumealsa_load_icons(PRIV(p));
    volumealsa_update_display(PRIV(p), TRUE);
}

static void volumealsa_on_volume_control_activate(GtkMenuItem * item, VolumeALSAPlugin * vol)
{
    wtl_launch(volumealsa_get_volume_control_command(vol), NULL);
}

static void volumealsa_popup_menu_hook(struct _Plugin * plugin, GtkMenu * menu)
{
    VolumeALSAPlugin * vol = PRIV(plugin);
    const char * command = volumealsa_get_volume_control_command(vol);
    if (command)
    {
        GtkWidget * mi = gtk_menu_item_new_with_label(_("Volume Control..."));
        gchar * tooltip = g_strdup_printf(_("Run %s"), command);
        gtk_widget_set_tooltip_text(mi, tooltip);
        g_free(tooltip);
        g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(volumealsa_on_volume_control_activate), vol);
        gtk_widget_show(mi);
        gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), mi);
    }
}

/* Plugin descriptor. */
PluginClass volumealsa_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "volumealsa",
    name : N_("Volume Control (ALSA)"),
    version: VERSION,
    description : N_("Display and control volume for ALSA"),
    category: PLUGIN_CATEGORY_HW_INDICATOR,

    constructor : volumealsa_constructor,
    destructor  : volumealsa_destructor,
    config : volumealsa_configure,
    save : volumealsa_save_configuration,
    panel_configuration_changed : volumealsa_panel_configuration_changed,
    popup_menu_hook : volumealsa_popup_menu_hook
};
