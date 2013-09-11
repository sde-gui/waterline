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

#include <waterline/dbg.h>

extern void gtk_run(void); /* FIXME! */

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
} menup;

static guint idle_loader = 0;

GQuark SYS_MENU_ITEM_ID = 0;

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

    g_free(m->fname);
    g_free(m->caption);
    g_free(m);
    RET();
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


static void run_command(GtkWidget *widget, void (*cmd)(void))
{
    cmd();
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

static void on_menu_item( GtkMenuItem* mi, MenuCacheItem* item )
{
    wtl_launch_app( menu_cache_app_get_exec(MENU_CACHE_APP(item)),
            NULL, menu_cache_app_get_use_terminal(MENU_CACHE_APP(item)));
}

/* load icon when mapping the menu item to speed up */
static void on_menu_item_map(GtkWidget* mi, MenuCacheItem* item)
{
    GtkImage* img = GTK_IMAGE(gtk_image_menu_item_get_image(GTK_IMAGE_MENU_ITEM(mi)));
    if( img )
    {
        if( gtk_image_get_storage_type(img) == GTK_IMAGE_EMPTY )
        {
            GdkPixbuf* icon;
            int w, h;
            /* FIXME: this is inefficient */
            gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
            item = g_object_get_qdata(G_OBJECT(mi), SYS_MENU_ITEM_ID);
            icon = wtl_load_icon(menu_cache_item_get_icon(item), w, h, TRUE);
            if (icon)
            {
                gtk_image_set_from_pixbuf(img, icon);
                g_object_unref(icon);
            }
        }
    }
}

static void on_menu_item_style_set(GtkWidget* mi, GtkStyle* prev, MenuCacheItem* item)
{
    /* reload icon */
    on_menu_item_map(mi, item);
}

static void on_add_menu_item_to_desktop(GtkMenuItem* item, MenuCacheApp* app)
{
    char* dest;
    char* src;
    const char* desktop = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
    int dir_len = strlen(desktop);
    int basename_len = strlen(menu_cache_item_get_id(MENU_CACHE_ITEM(app)));
    int dest_fd;

    dest = g_malloc( dir_len + basename_len + 6 + 1 + 1 );
    memcpy(dest, desktop, dir_len);
    dest[dir_len] = '/';
    memcpy(dest + dir_len + 1, menu_cache_item_get_id(MENU_CACHE_ITEM(app)), basename_len + 1);

    /* if the destination file already exists, make a unique name. */
    if( g_file_test( dest, G_FILE_TEST_EXISTS ) )
    {
        memcpy( dest + dir_len + 1 + basename_len - 8 /* .desktop */, "XXXXXX.desktop", 15 );
        dest_fd = g_mkstemp(dest);
        if( dest_fd >= 0 )
            chmod(dest, 0600);
    }
    else
    {
        dest_fd = creat(dest, 0600);
    }

    if( dest_fd >=0 )
    {
        char* data;
        gsize len;
        src = menu_cache_item_get_file_path(MENU_CACHE_ITEM(app));
        if( g_file_get_contents(src, &data, &len, NULL) )
        {
            write( dest_fd, data, len );
            g_free(data);
        }
        close(dest_fd);
        g_free(src);
    }
    g_free(dest);
}


static Plugin * get_launchbar_plugin(void)
{
    /* Find a penel containing launchbar applet.
     * The launchbar with most buttons will be choosen if
     * there are several launchbar applets loaded.
     */

    GSList * l;
    Plugin * lb = NULL;
    int prio = -1;

    for (l = get_all_panels(); !lb && l; l = l->next)
    {
        Panel * panel = (Panel *) l->data;
        GList * pl;
        for (pl = panel_get_plugins(panel); pl; pl = pl->next)
        {
            Plugin* plugin = (Plugin *) pl->data;
            if (plugin_class(plugin)->add_launch_item && plugin_class(plugin)->get_priority_of_launch_item_adding)
            {
                int n = plugin_class(plugin)->get_priority_of_launch_item_adding(plugin);
                if( n > prio )
                {
                    lb = plugin;
                    prio = n;
                }
            }
        }
    }

    return lb;
}


static void on_add_menu_item_to_panel(GtkMenuItem* item, MenuCacheApp* app)
{
    /*
    FIXME: let user choose launchbar
    */

    Plugin * lb = get_launchbar_plugin();
    if (lb)
    {
        plugin_class(lb)->add_launch_item(lb, menu_cache_item_get_file_basename(MENU_CACHE_ITEM(app)));
    }
}

static void on_menu_item_properties(GtkMenuItem* item, MenuCacheApp* app)
{
    /* FIXME: if the source desktop is in AppDir other then default
     * applications dirs, where should we store the user-specific file?
    */
    char* ifile = menu_cache_item_get_file_path(MENU_CACHE_ITEM(app));
    char* ofile = g_build_filename(g_get_user_data_dir(), "applications",
				   menu_cache_item_get_file_basename(MENU_CACHE_ITEM(app)), NULL);
    char* argv[] = {
        "lxshortcut",
        "-i",
        NULL,
        "-o",
        NULL,
        NULL};
    argv[2] = ifile;
    argv[4] = ofile;
    g_spawn_async( NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL );
    g_free( ifile );
    g_free( ofile );
}

static gboolean on_menu_button_press(GtkWidget* mi, GdkEventButton* evt, MenuCacheItem* data)
{
    if( evt->button == 3)  /* right */
    {
        if (wtl_is_in_kiosk_mode())
            return TRUE;

        char* tmp;
        GtkWidget* item;
        GtkMenu* p = GTK_MENU(gtk_menu_new());

        item = gtk_menu_item_new_with_label(_("Add to desktop"));
        g_signal_connect(item, "activate", G_CALLBACK(on_add_menu_item_to_desktop), data);
        gtk_menu_shell_append(GTK_MENU_SHELL(p), item);

        if (get_launchbar_plugin())
        {
            item = gtk_menu_item_new_with_label(_("Add to launch bar"));
            g_signal_connect(item, "activate", G_CALLBACK(on_add_menu_item_to_panel), data);
            gtk_menu_shell_append(GTK_MENU_SHELL(p), item);
        }

        tmp = g_find_program_in_path("lxshortcut");
        if( tmp )
        {
            item = gtk_separator_menu_item_new();
            gtk_menu_shell_append(GTK_MENU_SHELL(p), item);

            item = gtk_menu_item_new_with_label(_("Properties"));
            g_signal_connect(item, "activate", G_CALLBACK(on_menu_item_properties), data);
            gtk_menu_shell_append(GTK_MENU_SHELL(p), item);
            g_free(tmp);
        }
        g_signal_connect(p, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);
        g_signal_connect(p, "deactivate", G_CALLBACK(restore_grabs), mi);

        gtk_widget_show_all(GTK_WIDGET(p));
        gtk_menu_popup(p, NULL, NULL, NULL, NULL, 0, evt->time);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_menu_button_release(GtkWidget* mi, GdkEventButton* evt, MenuCacheItem* data)
{
    if( evt->button == 3)
    {
        return TRUE;
    }

    return FALSE;
}

static char * str_remove_trailing_percent_args(char * s)
{
    if (!s)
        return NULL;

    while(1)
    {
        s = g_strstrip(s);
        int l = strlen(s);
        if (l > 4 && s[l - 3] == ' ' && s[l - 2] == '%')
            s[l - 3] = 0;
        else
            break;
    }

    return s;
}

static GtkWidget* create_item( MenuCacheItem* item )
{
    GtkWidget* mi;
    if( menu_cache_item_get_type(item) == MENU_CACHE_TYPE_SEP )
        mi = gtk_separator_menu_item_new();
    else
    {
        const char * name = menu_cache_item_get_name(item);
        su_log_debug("Name    = %s", name);
        if (!name)
            name = "<unknown>";
        mi = gtk_image_menu_item_new_with_label(name);
        GtkWidget * img = gtk_image_new();
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(mi), img );
        if( menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP )
        {
            const gchar * tooltip = menu_cache_item_get_comment(item);
            su_log_debug("Tooltip = %s", tooltip);

/*
            FIXME: to be implemented in menu-cache
            if (su_str_empty(tooltip))
                tooltip = menu_cache_item_get_generic_name(item);
*/
            gchar * additional_tooltip = NULL;

            const gchar * commandline = menu_cache_app_get_exec(MENU_CACHE_APP(item));
            su_log_debug("Exec    = %s", commandline);

            gchar * executable = str_remove_trailing_percent_args(g_strdup(commandline));

            if (executable)
            {
                if (su_str_empty(tooltip))
                {
                    additional_tooltip = g_strdup(executable);
                }
                else
                {
                    gchar * s0 = g_ascii_strdown(executable, -1);
                    gchar * s1 = g_ascii_strdown(tooltip, -1);
                    gchar * s2 = g_ascii_strdown(menu_cache_item_get_name(item), -1);
                    if (!strstr(s1, s0) && !strstr(s2, s0))
                    {
                        additional_tooltip = g_strdup_printf(_("%s\n[%s]"), tooltip, executable);
                    }
                }
            }

            g_free(executable);

            if (additional_tooltip)
            {
                gtk_widget_set_tooltip_text(mi, additional_tooltip);
                g_free(additional_tooltip);
            }
            else
            {
               gtk_widget_set_tooltip_text(mi, tooltip);
            }

            g_signal_connect( mi, "activate", G_CALLBACK(on_menu_item), item );
        }
        g_signal_connect(mi, "map", G_CALLBACK(on_menu_item_map), item);
        g_signal_connect(mi, "style-set", G_CALLBACK(on_menu_item_style_set), item);
        g_signal_connect(mi, "button-press-event", G_CALLBACK(on_menu_button_press), item);
        g_signal_connect(mi, "button-release-event", G_CALLBACK(on_menu_button_release), item);
    }
    gtk_widget_show( mi );
    g_object_set_qdata_full( G_OBJECT(mi), SYS_MENU_ITEM_ID, menu_cache_item_ref(item), (GDestroyNotify) menu_cache_item_unref );
    return mi;
}

static int load_menu(menup* m, MenuCacheDir* dir, GtkWidget* menu, int pos )
{
    GSList * l;
    /* number of visible entries */
    gint count = 0;		
    for( l = menu_cache_dir_get_children(dir); l; l = l->next )
    {
        MenuCacheItem* item = MENU_CACHE_ITEM(l->data);
	
        gboolean is_visible = ((menu_cache_item_get_type(item) != MENU_CACHE_TYPE_APP) || 
			       (panel_menu_item_evaluate_visibility(item, m->visibility_flags)));
	
	if (is_visible) 
	{
            GtkWidget * mi = create_item(item);
	    count++;
            if (mi != NULL)
                gtk_menu_shell_insert( (GtkMenuShell*)menu, mi, pos );
                if( pos >= 0 )
                    ++pos;
		/* process subentries */
		if (menu_cache_item_get_type(item) == MENU_CACHE_TYPE_DIR) 
		{
                    GtkWidget* sub = gtk_menu_new();
		    /*  always pass -1 for position */
		    gint s_count = load_menu( m, MENU_CACHE_DIR(item), sub, -1 );    
                    if (s_count) 
			gtk_menu_item_set_submenu( GTK_MENU_ITEM(mi), sub );	    
		    else 
		    {
			/* don't keep empty submenus */
			gtk_widget_destroy( sub );
			gtk_widget_destroy( mi );
			if (pos > 0)
			    pos--;
		    }
		}
	}
    }
    return count;
}



static gboolean sys_menu_item_has_data( GtkMenuItem* item )
{
   return (g_object_get_qdata( G_OBJECT(item), SYS_MENU_ITEM_ID ) != NULL);
}

static void unload_old_icons(GtkMenu* menu, GtkIconTheme* theme)
{
    GList *children, *child;
    GtkMenuItem* item;
    GtkWidget* sub_menu=NULL;

    children = gtk_container_get_children( GTK_CONTAINER(menu) );
    for( child = children; child; child = child->next )
    {
        item = GTK_MENU_ITEM( child->data );
        if( sys_menu_item_has_data( item ) )
        {
            GtkImage* img;
            item = GTK_MENU_ITEM( child->data );
            if( GTK_IS_IMAGE_MENU_ITEM(item) )
            {
	        img = GTK_IMAGE(gtk_image_menu_item_get_image(GTK_IMAGE_MENU_ITEM(item)));
                gtk_image_clear(img);
                if( gtk_widget_get_mapped(GTK_WIDGET(img)) )
		    on_menu_item_map(GTK_WIDGET(item),
			(MenuCacheItem*)g_object_get_qdata(G_OBJECT(item), SYS_MENU_ITEM_ID) );
            }
        }
        else if( ( sub_menu = gtk_menu_item_get_submenu( item ) ) )
        {
	    unload_old_icons( GTK_MENU(sub_menu), theme );
        }
    }
    g_list_free( children );
}

static void remove_change_handler(gpointer id, GObject* menu)
{
    g_signal_handler_disconnect(gtk_icon_theme_get_default(), GPOINTER_TO_INT(id));
}

/*
 * Insert application menus into specified menu
 * menu: The parent menu to which the items should be inserted
 * pisition: Position to insert items.
             Passing -1 in this parameter means append all items
             at the end of menu.
 */
static void sys_menu_insert_items( menup* m, GtkMenu* menu, int position )
{
    MenuCacheDir* dir;
    guint change_handler;

    if( G_UNLIKELY( SYS_MENU_ITEM_ID == 0 ) )
        SYS_MENU_ITEM_ID = g_quark_from_static_string( "SysMenuItem" );

    dir = menu_cache_get_root_dir( m->menu_cache );
    if(dir)
        load_menu( m, dir, GTK_WIDGET(menu), position );
    else /* menu content is empty */
    {
        /* add a place holder */
        GtkWidget* mi = gtk_menu_item_new();
        g_object_set_qdata( G_OBJECT(mi), SYS_MENU_ITEM_ID, GINT_TO_POINTER(1) );
        gtk_menu_shell_insert(GTK_MENU_SHELL(menu), mi, position);
    }

    change_handler = g_signal_connect_swapped( gtk_icon_theme_get_default(), "changed", G_CALLBACK(unload_old_icons), menu );
    g_object_weak_ref( G_OBJECT(menu), remove_change_handler, GINT_TO_POINTER(change_handler) );
}


static void
reload_system_menu( menup* m, GtkMenu* menu )
{
    GList *children, *child;
    GtkMenuItem* item;
    GtkWidget* sub_menu;
    gint idx;
    //gboolean found = FALSE;

    children = gtk_container_get_children( GTK_CONTAINER(menu) );
    for( child = children, idx = 0; child; child = child->next, ++idx )
    {
        item = GTK_MENU_ITEM( child->data );
        if( sys_menu_item_has_data( item ) )
        {
            do
            {
                item = GTK_MENU_ITEM( child->data );
                child = child->next;
                gtk_widget_destroy( GTK_WIDGET(item) );
            }while( child && sys_menu_item_has_data( child->data ) );
            sys_menu_insert_items( m, menu, idx );
            if( ! child )
                break;
            //found = TRUE;
        }
        else if( ( sub_menu = gtk_menu_item_get_submenu( item ) ) )
        {
            reload_system_menu( m, GTK_MENU(sub_menu) );
        }
    }
    g_list_free( children );
}

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
    ENTER;

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
            gtk_run();
        }
        else
        {
            show_menu( widget, plugin, event->button, event->time );
        }
    }
    RET(TRUE);
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

    ENTER;
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

    RET(m->img);
}


static GtkWidget * read_item(Plugin *p, json_t * json_item)
{
    ENTER;

    menup * m = PRIV(p);

    GtkWidget * item;
    Command *cmd_entry = NULL;

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

    Command * tmp = NULL;

    for (tmp = commands; tmp->name; tmp++)
    {
        if (g_strcmp0(action, tmp->name) == 0)
        {
            cmd_entry = tmp;
            break;
        }
    }

    if (cmd_entry) /* built-in commands */
    {
        item = gtk_image_menu_item_new_with_label( _(cmd_entry->disp_name) );
        g_signal_connect(G_OBJECT(item), "activate", (GCallback)run_command, cmd_entry->cmd);
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

static void on_reload_menu( MenuCache* cache, menup* m )
{
    reload_system_menu( m, GTK_MENU(m->menu) );
}

#include "menu_recent_documents.c"

static void
read_system_menu(GtkMenu* menu, Plugin *p, json_t * json_item)
{
    menup *m = PRIV(p);

    if (m->menu_cache == NULL)
    {
        guint32 flags;
        m->menu_cache = panel_menu_cache_new(&flags);
        if (m->menu_cache == NULL)
        {
            su_print_error_message("error loading applications menu");
            return;
        }
        m->visibility_flags = flags;
        m->reload_notify = menu_cache_add_reload_notify(m->menu_cache, (MenuCacheReloadNotify) on_reload_menu, m);
    }

    sys_menu_insert_items( m, menu, -1 );
    plugin_set_has_system_menu(p, TRUE);
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

        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL (menu), mi);

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

PluginClass menu_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "menu",
    name : N_("Menu"),
    version: "2.0",
    description : N_("Application Menu"),

    constructor : menu_constructor,
    destructor  : menu_destructor,
    panel_configuration_changed : menu_panel_configuration_changed,
    open_system_menu : menu_open_system_menu
};

