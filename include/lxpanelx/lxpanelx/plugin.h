/**
 * Copyright (c) 2011-2012 Vadim Ushakov
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

#ifndef _LXPANELX_PLUGIN_H
#define _LXPANELX_PLUGIN_H

#include <gmodule.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdio.h>

#include "typedef.h"

/* Support for external plugin versioning.
 * Plugins must invoke PLUGINCLASS_VERSIONING when they instantiate PluginClass. */
#define PLUGINCLASS_MAGIC 0x7bd4370f
#define PLUGINCLASS_VERSION 1
#define PLUGINCLASS_BASE_SIZE ((unsigned short) (unsigned) & ((PluginClass*)0)->__end_of_required_part)
#define PLUGINCLASS_VERSIONING \
    structure_magic : PLUGINCLASS_MAGIC, \
    structure_version : PLUGINCLASS_VERSION, \
    structure_subversion : 0, \
    structure_base_size : PLUGINCLASS_BASE_SIZE, \
    structure_actual_size : sizeof(PluginClass)

/* Representative of an available plugin. */
struct _PluginClass {

    /* Keep these first.  Do not make unnecessary changes in structure layout. */
    unsigned long  structure_magic;
    unsigned short structure_version;		/* Structure version. To distinguish backward-incompatible versions of structure. */
    unsigned short structure_subversion;	/* Structure subversion. Reserved. */
    unsigned short structure_base_size;		/* Size of required part. */
    unsigned short structure_actual_size;	/* Actual size of structure.  */

    PluginClassInternal * internal;

    int dynamic : 1;				/* True if dynamically loaded */
    int unused_invisible : 1;			/* Unused; reserved bit */
    int not_unloadable : 1;			/* Not unloadable due to GModule restriction */
    int one_per_system : 1;			/* Special: only one possible per system, such as system tray */
    int one_per_system_instantiated : 1;	/* True if one instance exists */
    int expand_available : 1;			/* True if "stretch" option is available */
    int expand_default : 1;			/* True if "stretch" option is default */

    /* These fields point within the plugin image. */
    char * type;				/* Internal name of plugin, to match external filename */
    char * name;				/* Display name of plugin for selection UI */
    char * version;				/* Version of plugin */
    char * description;				/* Brief textual description of plugin for selection UI */

    int  (*constructor)(struct _Plugin * plugin, char ** fp);		/* Create an instance of the plugin */
    void (*destructor)(struct _Plugin * plugin);			/* Destroy an instance of the plugin */

    void * __end_of_required_part;

    void (*config)(struct _Plugin * plugin, GtkWindow * parent);	/* Request the plugin to display its configuration dialog */
    void (*save)(struct _Plugin * plugin, FILE * fp);			/* Request the plugin to save its configuration to a file */
    void (*panel_configuration_changed)(struct _Plugin * plugin);	/* Request the plugin to do a full redraw after a panel configuration change */
    void (*run_command)(struct _Plugin * plugin, char ** argv, int argc);
    void (*open_system_menu)(struct _Plugin * plugin);
    void (*add_launch_item)(struct _Plugin * plugin, const char * name);
    int  (*get_priority_of_launch_item_adding)(struct _Plugin * plugin);
    void (*popup_menu_hook)(struct _Plugin * plugin, GtkMenu * menu);
};



extern Panel * plugin_panel(Plugin * plugin);
extern PluginClass * plugin_class(Plugin * plugin);

extern void * plugin_priv(Plugin * plugin);
extern void plugin_set_priv(Plugin * plugin, void * priv);

#ifdef PLUGIN_PRIV_TYPE
#define PRIV(p) ( (PLUGIN_PRIV_TYPE * ) plugin_priv(p))
#endif

extern GtkWidget * plugin_widget(Plugin * plugin);;
extern void plugin_set_widget(Plugin * plugin, GtkWidget * widget);

extern void plugin_set_has_system_menu(Plugin * plugin, gboolean v);


extern int plugin_get_icon_size(Plugin * plugin);
extern int plugin_get_orientation(Plugin * plugin);


extern GtkMenu * plugin_get_menu(Plugin * plugin, gboolean use_sub_menu);
extern void plugin_show_menu(Plugin * plugin, GdkEventButton * event);


extern gboolean plugin_button_press_event(GtkWidget *widget, GdkEventButton *event, Plugin *plugin);
                                                        /* Handler for "button_press_event" signal with Plugin as parameter */

extern void plugin_popup_set_position_helper2(Plugin * p, GtkWidget * near, GtkWidget * popup, GtkRequisition * popup_req, int offset, float alignment, gint * px, gint * py);

extern void plugin_popup_set_position_helper(Plugin * p, GtkWidget * near, GtkWidget * popup, GtkRequisition * popup_req, gint * px, gint * py);
							/* Helper for position-calculation callback for popup menus */
extern void plugin_adjust_popup_position(GtkWidget * popup, Plugin * plugin);
							/* Helper to move popup windows away from the panel */

extern void plugin_lock_visible(Plugin * plugin);
extern void plugin_unlock_visible(Plugin * plugin);

extern void plugin_run_command(Plugin * plugin, char ** argv, int argc);

#endif
