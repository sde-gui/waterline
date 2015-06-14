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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <waterline/menu-cache-compat.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <sde-utils.h>
#include <sde-utils-jansson.h>

#define PLUGIN_PRIV_TYPE menup

#include <waterline/symbol_visibility.h>
#include <waterline/gtkcompat.h>
#include <waterline/global.h>
#include <waterline/panel.h>
#include <waterline/misc.h>
#include <waterline/plugin.h>
#include <waterline/paths.h>
#include <waterline/fb_button.h>
#include "bg.h"
#include "menu-policy.h"
#include "commands.h"

#define DEFAULT_MENU_ICON "start-here"



/* Test of haystack has the needle prefix, comparing case
 * insensitive. haystack may be UTF-8, but needle must
 * contain only lowercase ascii. */
static gboolean
has_case_prefix (const gchar *haystack,
                 const gchar *needle)
{
  const gchar *h, *n;

  /* Eat one character at a time. */
  h = haystack;
  n = needle;

  while (*n && *h && *n == g_ascii_tolower (*h))
    {
      n++;
      h++;
    }

  return *n == '\0';
}


typedef struct {
    GtkWidget *menu, *box, *img, *label;
    char *fname, *caption;
    gulong handler_id;
    int iconsize, paneliconsize;
    GSList *files;
    char* config_data;
    int sysmenu_pos;
    char *config_start, *config_end;

    MenuCache* menu_cache;
    guint visibility_flags;
    gpointer reload_notify;

    gboolean has_run_command;

    guint menu_reload_timeout_cb;
} menup;

static guint idle_loader = 0;

static void
menu_destructor(Plugin *p)
{
    menup *m = PRIV(p);

    if( G_UNLIKELY( idle_loader ) )
    {
        g_source_remove( idle_loader );
        idle_loader = 0;
    }

    g_signal_handler_disconnect(G_OBJECT(m->img), m->handler_id);
    gtk_widget_destroy(m->menu);

    if( m->menu_cache )
    {
        menu_cache_remove_reload_notify(m->menu_cache, m->reload_notify);
        menu_cache_unref( m->menu_cache );
    }

    if (m->menu_reload_timeout_cb)
        g_source_remove(m->menu_reload_timeout_cb);

    g_free(m->fname);
    g_free(m->caption);
    g_free(m);
}

static void spawn_app(GtkWidget *widget, gpointer data)
{
    GError *error = NULL;

    if (!data)
        return;

    if (! g_spawn_command_line_async(data, &error) )
    {
        su_print_error_message("can't spawn %s\nError is %s\n", (char *)data, error->message);
        g_error_free (error);
    }
}


static void run_command(GtkWidget *widget, const char * command)
{
    wtl_command_run(command);
}

static void menu_pos(GtkWidget *menu, gint *px, gint *py, gboolean *push_in, Plugin * p)
{
    /* Get the allocation of the popup menu. */
    GtkRequisition popup_req;
    gtk_widget_size_request(menu, &popup_req);

    /* Determine the coordinates. */
    plugin_popup_set_position_helper(p, plugin_widget(p), menu, &popup_req, px, py);
    *push_in = TRUE;
}

#include "menu_xdg.c"
#include "menu_recent_documents.c"

static void show_menu( GtkWidget* widget, Plugin* p, int btn, guint32 time )
{
    menup* m = PRIV(p);
    gtk_menu_popup(GTK_MENU(m->menu),
                   NULL, NULL,
                   (GtkMenuPositionFunc)menu_pos, p,
                   btn, time);
}

static gboolean
my_button_pressed(GtkWidget *widget, GdkEventButton *event, Plugin* plugin)
{
    menup *m = PRIV(plugin);

    /* Standard right-click handling. */
    if (plugin_button_press_event(widget, event, plugin))
        return TRUE;

    if ((event->type == GDK_BUTTON_PRESS)
          && (event->x >=0 && event->x < widget->allocation.width)
          && (event->y >=0 && event->y < widget->allocation.height))
    {
        if (m->has_run_command && event->button == 2)
        {
            wtl_show_run_box();
        }
        else
        {
            show_menu( widget, plugin, event->button, event->time );
        }
    }
    return TRUE;
}

static
void menu_open_system_menu(struct _Plugin * p)
{
    menup* m = PRIV(p);
    show_menu( m->img, p, 0, GDK_CURRENT_TIME );
}

static GtkWidget *
make_button(Plugin *p, gchar *fname, gchar *name, GdkColor* tint, GtkWidget *menu)
{
    char* title = NULL;
    menup *m;

    m = PRIV(p);
    m->menu = menu;

    if( name )
    {
        title = panel_translate_directory_name(name);
        m->img = fb_button_new_from_file_with_text(fname, -1, plugin_get_icon_size(p), p, title);
        g_free(title);
    }
    else
    {
        m->img = fb_button_new_from_file(fname, -1, plugin_get_icon_size(p), p);
    }

    fb_button_set_orientation(m->img, plugin_get_orientation(p));

    gtk_widget_show(m->img);
    gtk_box_pack_start(GTK_BOX(m->box), m->img, TRUE, TRUE, 0);

    m->handler_id = g_signal_connect (G_OBJECT (m->img), "button-press-event",
          G_CALLBACK (my_button_pressed), p);
    g_object_set_data(G_OBJECT(m->img), "plugin", p);

    return m->img;
}


static GtkWidget * read_item(Plugin *p, json_t * json_item)
{
    menup * m = PRIV(p);

    GtkWidget * item;

    gchar * name = NULL;
    gchar * icon = NULL;
    gchar * action = NULL;
    su_json_dot_get_string(json_item, "name", "<unknown>", &name);
    su_json_dot_get_string(json_item, "icon", "", &icon);
    su_json_dot_get_string(json_item, "action", "", &action);

    if (!su_str_empty(icon))
    {
        gchar * icon1 = su_path_expand_tilda(icon);
        g_free(icon);
        icon = icon1;
    }

    if (!g_ascii_strcasecmp(action, "run"))
    {
        m->has_run_command = TRUE;
    }

    if (wtl_command_exists(action))
    {
        const char * displayed_name = wtl_command_get_displayed_name(action);
        if (!displayed_name)
            displayed_name = action;
        item = gtk_image_menu_item_new_with_label(displayed_name);
        g_signal_connect(G_OBJECT(item), "activate", (GCallback)run_command, (gpointer) wtl_command_get_const_name(action));
    }
    else
    {
        item = gtk_image_menu_item_new_with_label(name ? name : "");
        if (!su_str_empty(action))
        {
            g_signal_connect(G_OBJECT(item), "activate", (GCallback)spawn_app, action);
        }
        else
        {
            gtk_widget_set_sensitive(item, FALSE);
        }
    }

    gtk_container_set_border_width(GTK_CONTAINER(item), 0);

    if (!su_str_empty(icon)) {
        GtkWidget *image = _gtk_image_new_from_file_scaled(icon, m->iconsize, m->iconsize, TRUE, TRUE);
        gtk_widget_show(image);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);

    }

    g_free(name);
    g_free(icon);
    g_free(action);

    return item;
}

static GtkWidget * read_separator(Plugin *p, json_t * json_separator)
{
    return gtk_separator_menu_item_new();
}

static GtkWidget *
read_submenu(Plugin *p, json_t * json_menu, gboolean as_item)
{
    GtkWidget *mi, *menu;
    menup *m = PRIV(p);

    menu = gtk_menu_new ();
    gtk_container_set_border_width(GTK_CONTAINER(menu), 0);

    json_t * json_items = json_object_get(json_menu, "items");

    size_t index;
    json_t * json_item = NULL;
    json_array_foreach(json_items, index, json_item) {
        mi = NULL;
        gchar * type = NULL;
        su_json_dot_get_string(json_item, "type", "", &type);
        if (!g_strcmp0(type, "item"))
        {
            mi = read_item(p, json_item);
        }
        else if (!g_strcmp0(type, "xdg_menu"))
        {
            read_system_menu(GTK_MENU(menu), p, json_item);
        }
        else if (!g_strcmp0(type, "recent_documents_menu"))
        {
            read_recent_documents_menu(GTK_MENU(menu), p, json_item);
        }
        else if (!g_strcmp0(type, "separator"))
        {
            mi = read_separator(p, json_item);
        }
        else if (!g_strcmp0(type, "menu"))
        {
            mi = read_submenu(p, json_item, TRUE);
        }

        if (mi)
        {
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL (menu), mi);
        }

        g_free(type);
    }

    if (as_item)
    {
        gchar * icon = NULL;
        su_json_dot_get_string(json_menu, "icon", "", &icon);
        gchar * name = NULL;
        su_json_dot_get_string(json_menu, "name", "<unknown>", &name);

        mi = gtk_image_menu_item_new_with_label(name);
        if (icon) {
            GtkWidget * image = _gtk_image_new_from_file_scaled(icon, m->iconsize, m->iconsize, TRUE, TRUE);
            gtk_widget_show(image);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), image);
        }
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), menu);
        g_free(name);
        g_free(icon);
    }
    else
    {
        su_json_dot_get_string(json_menu, "icon", m->fname, &m->fname);
        su_json_dot_get_string(json_menu, "name", m->caption, &m->caption);
        mi = make_button(p, m->fname, m->caption, NULL, menu);
        return mi;
    }
    return mi;
}

static int
menu_constructor(Plugin *p)
{
    menup *m;
    int iw, ih;

    m = g_new0(menup, 1);
    g_return_val_if_fail(m != NULL, 0);

    m->caption = NULL;
    m->fname = g_strdup(DEFAULT_MENU_ICON);

    plugin_set_priv(p, m);

    gtk_icon_size_lookup( GTK_ICON_SIZE_MENU, &iw, &ih );
    m->iconsize = MAX(iw, ih);

    m->box = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(m->box), 0);

    json_t * json_menu = json_incref(json_object_get(plugin_inner_json(p), "menu"));
    if (!json_menu) {
        gchar * path = wtl_get_config_path("plugins/menu/application_menu.js", SU_PATH_CONFIG_USER);
        if (path) {
            json_menu = json_load_file(path, 0, NULL);
        }
    }

    if (!read_submenu(p, json_menu, FALSE)) {
        su_print_error_message("menu: plugin init failed\n");
        return 0;
    }

    json_decref(json_menu);

    plugin_set_widget(p, m->box);

    return 1;
}

static void apply_config(Plugin* p)
{
    menup* m = PRIV(p);
    fb_button_set_orientation(m->img, plugin_get_orientation(p));
}

/* Callback when panel configuration changes. */
static void menu_panel_configuration_changed(Plugin * p)
{
    apply_config(p);
}

SYMBOL_PLUGIN_CLASS PluginClass menu_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "menu",
    name : N_("Menu"),
    version: "2.0",
    description : N_("Application Menu"),
    category: PLUGIN_CATEGORY_LAUNCHER,

    constructor : menu_constructor,
    destructor  : menu_destructor,
    panel_configuration_changed : menu_panel_configuration_changed,
    open_system_menu : menu_open_system_menu
};

