/**
 * Copyright (c) 2015 Vadim Ushakov
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>
#include <sde-utils-jansson.h>

#define PLUGIN_PRIV_TYPE SpacePlugin

#include <waterline/symbol_visibility.h>
#include <waterline/panel.h>
#include <waterline/misc.h>
#include <waterline/plugin.h>

typedef struct {
    int size;
    gboolean show_separator;
    GtkWidget * inner_box;
    GtkWidget * separator;
} SpacePlugin;

static int space_constructor(Plugin * p);
static void space_destructor(Plugin * p);
static void space_apply_configuration(Plugin * p);
static void space_configure(Plugin * p, GtkWindow * parent);
static void space_save_configuration(Plugin * p);

/******************************************************************************/

#define SU_JSON_OPTION_STRUCTURE SpacePlugin
static su_json_option_definition option_definitions[] = {
    SU_JSON_OPTION(int, size),
    SU_JSON_OPTION(bool, show_separator),
    {0,}
};

/******************************************************************************/

static int space_constructor(Plugin * p)
{
    SpacePlugin * sp = g_new0(SpacePlugin, 1);
    plugin_set_priv(p, sp);

    sp->show_separator = TRUE;

    su_json_read_options(plugin_inner_json(p), option_definitions, sp);

    if (sp->size < 1)
        sp->size = 1;

    GtkWidget * pwid = gtk_event_box_new();
    plugin_set_widget(p, pwid);
    gtk_widget_set_has_window(pwid, FALSE);
    gtk_widget_add_events(pwid, GDK_BUTTON_PRESS_MASK);
    gtk_container_set_border_width(GTK_CONTAINER(pwid), 0);

    g_signal_connect(pwid, "button-press-event", G_CALLBACK(plugin_button_press_event), p);

    space_apply_configuration(p);
    gtk_widget_show(pwid);
    return 1;
}

static void space_destructor(Plugin * p)
{
    SpacePlugin * sp = PRIV(p);
    g_free(sp);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static void space_apply_configuration(Plugin * p)
{
    SpacePlugin * sp = PRIV(p);
    GtkWidget * box = plugin_widget(p);

    if (sp->separator)
    {
        gtk_widget_destroy(sp->separator);
        sp->separator = NULL;
    }

    if (sp->inner_box)
    {
        gtk_widget_destroy(sp->inner_box);
        sp->inner_box = NULL;
    }

    if (sp->show_separator)
    {
        if (plugin_get_orientation(p) == ORIENT_HORIZ)
            sp->inner_box = gtk_hbox_new(FALSE, 0);
        else
            sp->inner_box = gtk_vbox_new(FALSE, 0);

        gtk_container_add(GTK_CONTAINER(box), sp->inner_box);
        gtk_widget_show(sp->inner_box);

        if (plugin_get_orientation(p) == ORIENT_HORIZ)
            sp->separator = gtk_vseparator_new();
        else
            sp->separator = gtk_hseparator_new();

        gtk_box_pack_start(GTK_BOX(sp->inner_box), sp->separator, TRUE, FALSE, 0);

        gtk_widget_show(sp->separator);
    }

    if (plugin_get_orientation(p) == ORIENT_HORIZ)
        gtk_widget_set_size_request(box, sp->size, -1);
    else
        gtk_widget_set_size_request(box, -1, sp->size);
}

static void space_panel_configuration_changed(Plugin * p)
{
    space_apply_configuration(p);
}

/* Callback when the configuration dialog is to be shown. */
static void space_configure(Plugin * p, GtkWindow * parent)
{
    SpacePlugin * sp = PRIV(p);
    GtkWidget * dialog = wtl_create_generic_config_dialog(
        _(plugin_class(p)->name),
        GTK_WIDGET(parent),
        (GSourceFunc) space_apply_configuration, (gpointer) p,
        _("Size"), &sp->size, (GType)CONF_TYPE_INT,
        _("Show a separator line"), &sp->show_separator, (GType)CONF_TYPE_BOOL,
        NULL);
    if (dialog)
    {
        //gtk_widget_set_size_request(GTK_WIDGET(dialog), 200, -1); /* Improve geometry */
        gtk_window_present(GTK_WINDOW(dialog));
    }
}

/* Callback when the configuration is to be saved. */
static void space_save_configuration(Plugin * p)
{
    SpacePlugin * sp = PRIV(p);
    su_json_write_options(plugin_inner_json(p), option_definitions, sp);
}

/* Plugin descriptor. */
SYMBOL_PLUGIN_CLASS PluginClass space_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "space",
    name : N_("Spacer"),
    version: VERSION,
    description : N_("The Spacer allows you to visually split the panel into sections."),

    /* Stretch is available but not default for this plugin. */
    expand_available : TRUE,

    constructor : space_constructor,
    destructor  : space_destructor,
    show_properties : space_configure,
    save_configuration : space_save_configuration,
    panel_configuration_changed : space_panel_configuration_changed
};
