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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>
#include <sde-utils-jansson.h>

#include <X11/XKBlib.h>

#define PLUGIN_PRIV_TYPE xkb_locks_t

#include <waterline/panel.h>
#include <waterline/misc.h>
#include <waterline/paths.h>
#include <waterline/plugin.h>

#include <waterline/gtkcompat.h>

static const char * on_icons[] = {
    "capslock-on.png",
    "numlock-on.png",
    "scrllock-on.png"
};

static const char * off_icons[] = {
    "capslock-off.png",
    "numlock-off.png",
    "scrllock-off.png"
};

static int xkb_event_base = 0;
static int xkb_error_base = 0;

typedef struct {
    Plugin * plugin;
    IconGrid * icon_grid;
    GtkWidget * grid_item[3];
    GtkWidget * indicator_image[3];
    gboolean visible[3];
    unsigned int modifier_mask[3];
    unsigned int locked_mods;
} xkb_locks_t;

static void xkb_locks_update_image(xkb_locks_t * xkb_locks, int i, unsigned int state);
static void xkb_locks_update_display(Plugin * p, unsigned int state, gboolean force);
static GdkFilterReturn xkb_locks_event_filter(GdkXEvent * gdkxevent, GdkEvent * event, Plugin * p);
static int xkb_locks_constructor(Plugin * p);
static void xkb_locks_destructor(Plugin * p);
static void xkb_locks_apply_configuration(Plugin * p);
static void xkb_locks_configure(Plugin * p, GtkWindow * parent);
static void xkb_locks_save_configuration(Plugin * p);
static void xkb_locks_panel_configuration_changed(Plugin * p);

/******************************************************************************/

#define SU_JSON_OPTION_STRUCTURE xkb_locks_t
static su_json_option_definition option_definitions[] = {
    SU_JSON_OPTION(bool, visible[0]),
    SU_JSON_OPTION(bool, visible[1]),
    SU_JSON_OPTION(bool, visible[2]),
    {0,}
};

/******************************************************************************/

static unsigned int xkb_mask_modifier(XkbDescPtr xkb, const char * name)
{
    int i;
    if (!xkb || !xkb->names)
        return 0;
    for (i = 0; i < XkbNumVirtualMods; i++)
    {
        char * mod_name = XGetAtomName(xkb->dpy, xkb->names->vmods[i]);

        /*{
            unsigned int mask;
            XkbVirtualModsToReal(xkb, 1 << i, &mask);
            g_printf("%d %s = 0x%x\n", i, mod_name, mask);
        }*/

        if (mod_name != NULL && strcmp(name, mod_name) == 0)
        {
            unsigned int mask;
            XkbVirtualModsToReal(xkb, 1 << i, &mask);
            return mask;
        }
    }
    return 0;
}

static void xkb_locks_init_masks(xkb_locks_t * xkb_locks)
{
    xkb_locks->modifier_mask[0] = LockMask;
    xkb_locks->modifier_mask[1] = 1 << 4;
    xkb_locks->modifier_mask[2] = 1 << 7;

    XkbDescPtr xkb;
    if ((xkb = XkbGetKeyboard(gdk_x11_get_default_xdisplay(), XkbAllComponentsMask, XkbUseCoreKbd)) != NULL)
    {
        unsigned int mask;
        mask = xkb_mask_modifier(xkb, "Lock");
        if (mask)
            xkb_locks->modifier_mask[0] = mask;
        mask = xkb_mask_modifier(xkb, "NumLock");
        if (mask)
            xkb_locks->modifier_mask[1] = mask;
        mask = xkb_mask_modifier(xkb, "ScrollLock");
        if (mask)
            xkb_locks->modifier_mask[2] = mask;
        XkbFreeKeyboard(xkb, 0, True);
    }
}

static void xkb_locks_update_image(xkb_locks_t * xkb_locks, int i, unsigned int state)
{
    gchar * file = wtl_resolve_own_resource("", "images", ((state) ? on_icons[i] : off_icons[i]), 0);
    panel_image_set_from_file(plugin_panel(xkb_locks->plugin), xkb_locks->indicator_image[i], file);
    g_free(file);
}


static void xkb_locks_update_display(Plugin * p, unsigned int locked_mods, gboolean force)
{
    xkb_locks_t * xkb_locks = PRIV(p);

    if (!force && xkb_locks->locked_mods == locked_mods) {
        return;
    }

    int i;
    for (i = 0; i < 3; i++)
    {
        unsigned int mask = xkb_locks->modifier_mask[i];
        if (force || (xkb_locks->locked_mods & mask) != (locked_mods & mask))
            xkb_locks_update_image(xkb_locks, i, !!(locked_mods & mask));
    }

    xkb_locks->locked_mods = locked_mods;
}

static GdkFilterReturn xkb_locks_event_filter(GdkXEvent * gdkxevent, GdkEvent * event, Plugin * p)
{
    XEvent * xev = (XEvent *) gdkxevent;
    if (xev->xany.type == xkb_event_base + XkbEventCode)
    {
        XkbEvent * xkbev = (XkbEvent *) xev;
        if (xkbev->any.xkb_type == XkbStateNotify)
            xkb_locks_update_display(p, xkbev->state.locked_mods, FALSE);
    }
    return GDK_FILTER_CONTINUE;
}

static gboolean xkb_locks_button_press_event(GtkWidget * widget, GdkEventButton * event, xkb_locks_t * xkb_locks)
{
    if (plugin_button_press_event(widget, event, xkb_locks->plugin))
        return TRUE;

    if (event->button == 1) {
        unsigned int mask = 0;
        int i;
        for (i =0; i < 3; i++)
        {
            if (widget == xkb_locks->grid_item[i]) {
                mask = xkb_locks->modifier_mask[i];
                break;
            }
        }
        XkbLockModifiers(gdk_x11_get_default_xdisplay(), XkbUseCoreKbd, mask, mask & ~xkb_locks->locked_mods);
    }

    return FALSE;
}


static int xkb_locks_constructor(Plugin * p)
{
    xkb_locks_t * xkb_locks = g_new0(xkb_locks_t, 1);
    xkb_locks->plugin = p;
    xkb_locks->visible[0] = TRUE;
    xkb_locks->visible[1] = TRUE;
    xkb_locks->visible[2] = FALSE;
    plugin_set_priv(p, xkb_locks);

    su_json_read_options(plugin_inner_json(p), option_definitions, xkb_locks);

    GtkWidget * pwid = gtk_event_box_new();
    plugin_set_widget(p, pwid);
    gtk_widget_set_has_window(pwid, FALSE);
    gtk_widget_add_events(pwid, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(pwid, "button-press-event", G_CALLBACK(plugin_button_press_event), p);

    GtkOrientation bo = (plugin_get_orientation(p) == ORIENT_HORIZ) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    xkb_locks->icon_grid = icon_grid_new(pwid, bo,
        plugin_get_icon_size(p), plugin_get_icon_size(p), 0, 0,
        panel_get_oriented_height_pixels(plugin_panel(p)));

    {
        int i;
        for (i = 0; i < 3; i++)
        {
            xkb_locks->grid_item[i] = gtk_event_box_new();
            gtk_container_set_border_width(GTK_CONTAINER(xkb_locks->grid_item[i]), 0);
            g_signal_connect(xkb_locks->grid_item[i], "button_press_event",
                G_CALLBACK(xkb_locks_button_press_event), (gpointer) xkb_locks);

            xkb_locks->indicator_image[i] = gtk_image_new();
            gtk_container_add(GTK_CONTAINER(xkb_locks->grid_item[i]), xkb_locks->indicator_image[i]);
            gtk_widget_show(xkb_locks->indicator_image[i]);

            icon_grid_add(xkb_locks->icon_grid, xkb_locks->grid_item[i], xkb_locks->visible[i]);
        }
    }

    /* Initialize Xkb extension if not yet done. */
    if (xkb_event_base == 0)
    {
        int opcode;
        int maj = XkbMajorVersion;
        int min = XkbMinorVersion;
        if (!XkbLibraryVersion(&maj, &min))
            return 0;
        if (!XkbQueryExtension(gdk_x11_get_default_xdisplay(), &opcode, &xkb_event_base, &xkb_error_base, &maj, &min))
            return 0;
    }

    xkb_locks_init_masks(xkb_locks);

    /* Add GDK event filter and enable XkbIndicatorStateNotify events. */
    gdk_window_add_filter(NULL, (GdkFilterFunc) xkb_locks_event_filter, p);
    if (!XkbSelectEvents(gdk_x11_get_default_xdisplay(), XkbUseCoreKbd, XkbStateNotifyMask, XkbStateNotifyMask))
        return 0;

    XkbStateRec xkbState;
    XkbGetState(gdk_x11_get_default_xdisplay(), XkbUseCoreKbd, &xkbState);
    xkb_locks_update_display(p, xkbState.locked_mods, TRUE);

    gtk_widget_show(pwid);
    return 1;
}

static void xkb_locks_destructor(Plugin * p)
{
    xkb_locks_t * xkb_locks = PRIV(p);

    /* Remove GDK event filter. */
    gdk_window_remove_filter(NULL, (GdkFilterFunc) xkb_locks_event_filter, p);
    icon_grid_free(xkb_locks->icon_grid);
    g_free(xkb_locks);
}

static void xkb_locks_apply_configuration(Plugin * p)
{
    xkb_locks_t * xkb_locks = PRIV(p);
    int i;
    for (i = 0; i < 3; i++)
        icon_grid_set_visible(xkb_locks->icon_grid, xkb_locks->grid_item[i], xkb_locks->visible[i]);
}

static void xkb_locks_configure(Plugin * p, GtkWindow * parent)
{
    xkb_locks_t * xkb_locks = PRIV(p);
    GtkWidget * dialog = create_generic_config_dialog(
        _(plugin_class(p)->name),
        GTK_WIDGET(parent),
        (GSourceFunc) xkb_locks_apply_configuration, (gpointer) p,
        _("Show Caps Lock Indicator")  , &xkb_locks->visible[0], (GType)CONF_TYPE_BOOL,
        _("Show Num Lock Indicator")   , &xkb_locks->visible[1], (GType)CONF_TYPE_BOOL,
        _("Show Scroll Lock Indicator"), &xkb_locks->visible[2], (GType)CONF_TYPE_BOOL,
        NULL);
    if (dialog)
    {
       gtk_window_present(GTK_WINDOW(dialog));
    }
}

static void xkb_locks_save_configuration(Plugin * p)
{
    xkb_locks_t * xkb_locks = PRIV(p);
    su_json_write_options(plugin_inner_json(p), option_definitions, xkb_locks);
}

static void xkb_locks_panel_configuration_changed(Plugin * p)
{
    xkb_locks_t * xkb_locks = PRIV(p);
    GtkOrientation bo = (plugin_get_orientation(p) == ORIENT_HORIZ) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    icon_grid_set_geometry(xkb_locks->icon_grid, bo,
        plugin_get_icon_size(p), plugin_get_icon_size(p), 0, 0,
        panel_get_oriented_height_pixels(plugin_panel(p)));

    xkb_locks_update_display(p, xkb_locks->locked_mods, TRUE);
}


/* Plugin descriptor. */
PluginClass xkb_locks_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "xkb_locks",
    name : N_("Keyboard Lock Key Indicator"),
    version: "1.0",
    description : N_("Displays state of Caps Lock, Num Lock and Scroll Lock modifiers and allows you to toggle them with your mouse."),
    category: PLUGIN_CATEGORY_SW_INDICATOR,

    constructor : xkb_locks_constructor,
    destructor  : xkb_locks_destructor,
    config : xkb_locks_configure,
    save : xkb_locks_save_configuration,
    panel_configuration_changed : xkb_locks_panel_configuration_changed
};
