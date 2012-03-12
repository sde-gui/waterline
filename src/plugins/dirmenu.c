/**
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

#include <stdlib.h>
#include <unistd.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <string.h>
#include <gio/gio.h>

#include "panel.h"
#include "misc.h"
#include "plugin.h"
#include "dbg.h"

enum {
    SORT_BY_NAME,
    SORT_BY_MTIME,
    SORT_BY_SIZE
};

static pair sort_by_pair[] = {
    { SORT_BY_NAME , "Name"  },
    { SORT_BY_MTIME, "MTime" },
    { SORT_BY_SIZE , "Size"  },
    { 0, NULL},
};

/* Temporary for sort of directory names. */
typedef struct _file_name {
    struct _file_name * flink;
    char * file_name;
    char * file_name_collate_key;
    char * path;
    struct stat stat_data;
    gboolean directory;
} FileName;

/* Private context for directory menu plugin. */
typedef struct {
    Plugin * plugin;			/* Back pointer to plugin */
    char * image;			/* Icon for top level widget */
    char * path;			/* Top level path for widget */
    char * name;			/* User's label for widget */
    GdkPixbuf * folder_icon;		/* Icon for folders */
    gboolean show_hidden;
    gboolean show_files;
    gboolean show_file_size;
    int max_file_count;
    int sort_directories;
    int sort_files;
    gboolean plain_view;
    gboolean show_icons;
} DirMenuPlugin;

static void dirmenu_menuitem_open_file(GtkWidget * item, Plugin * p);
static void dirmenu_menuitem_open_directory(GtkWidget * item, Plugin * p);
static void dirmenu_menuitem_open_in_terminal(GtkWidget * item, Plugin * p);
static void dirmenu_menuitem_select(GtkMenuItem * item, Plugin * p);
static void dirmenu_menuitem_deselect(GtkMenuItem * item, Plugin * p);
void dirmenu_menu_selection_done(GtkWidget * menu, Plugin * p);
static void dirmenu_popup_set_position(GtkWidget * menu, gint * px, gint * py, gboolean * push_in, Plugin * p);
static GtkWidget * dirmenu_create_menu(Plugin * p, const char * path, gboolean open_at_top);
static void dirmenu_show_menu(GtkWidget * widget, Plugin * p, int btn, guint32 time);
static gboolean dirmenu_button_press_event(GtkWidget * widget, GdkEventButton * event, Plugin * p);
static int dirmenu_constructor(Plugin * p, char ** fp);
static void dirmenu_destructor(Plugin * p);
//static void dirmenu_apply_configuration_to_children(GtkWidget * w, DirMenuPlugin * dm);
static void dirmenu_apply_configuration(Plugin * p);
static void dirmenu_configure(Plugin * p, GtkWindow * parent);
static void dirmenu_save_configuration(Plugin * p, FILE * fp);
static void dirmenu_panel_configuration_changed(Plugin * p);

/* Handler for activate event on file menu item. */
static void dirmenu_menuitem_open_file(GtkWidget * item, Plugin * p)
{
    lxpanel_open_in_file_manager(g_object_get_data(G_OBJECT(item), "path"));
}

/* Handler for activate event on popup Open menu item. */
static void dirmenu_menuitem_open_directory(GtkWidget * item, Plugin * p)
{
    lxpanel_open_in_file_manager(g_object_get_data(G_OBJECT(gtk_widget_get_parent(item)), "path"));
}

/* Handler for activate event on popup Open In Terminal menu item. */
static void dirmenu_menuitem_open_in_terminal(GtkWidget * item, Plugin * p)
{
    lxpanel_open_in_terminal(g_object_get_data(G_OBJECT(gtk_widget_get_parent(item)), "path"));
}

/* Handler for select event on popup menu item. */
static void dirmenu_menuitem_open_directory_plain(GtkWidget * item, Plugin * p)
{
    GtkMenu * parent = GTK_MENU(gtk_widget_get_parent(GTK_WIDGET(item)));
    char * path = g_build_filename(
        (char *) g_object_get_data(G_OBJECT(parent), "path"),
        (char *) g_object_get_data(G_OBJECT(item), "name"),
        NULL);
    lxpanel_open_in_file_manager(path);
    g_free(path);
}

/* Handler for select event on popup menu item. */
static void dirmenu_menuitem_select(GtkMenuItem * item, Plugin * p)
{
    GtkWidget * sub = gtk_menu_item_get_submenu(item);
    if (sub != NULL)
    {
        /* On first reference, populate the submenu using the parent directory and the item directory name. */
        GtkMenu * parent = GTK_MENU(gtk_widget_get_parent(GTK_WIDGET(item)));
        char * path = (char *) g_object_get_data(G_OBJECT(sub), "path");
        if (path == NULL)
        {
            path = g_build_filename(
                (char *) g_object_get_data(G_OBJECT(parent), "path"),
                (char *) g_object_get_data(G_OBJECT(item), "name"),
                NULL);
            sub = dirmenu_create_menu(p, path, TRUE);
            g_free(path);
            gtk_menu_item_set_submenu(item, sub);
        }
    }
}

/* Handler for deselect event on popup menu item. */
static void dirmenu_menuitem_deselect(GtkMenuItem * item, Plugin * p)
{
    /* Delete old menu on deselect to save resource. */
    gtk_menu_item_set_submenu(item, gtk_menu_new());
}

/* Handler for selection-done event on popup menu. */
void dirmenu_menu_selection_done(GtkWidget * menu, Plugin * p)
{
    gtk_widget_destroy(menu);
}

/* Position-calculation callback for popup menu. */
static void dirmenu_popup_set_position(GtkWidget * menu, gint * px, gint * py, gboolean * push_in, Plugin * p)
{
    /* Get the allocation of the popup menu. */
    GtkRequisition popup_req;
    gtk_widget_size_request(menu, &popup_req);

    /* Determine the coordinates. */
    plugin_popup_set_position_helper(p, p->pwid, menu, &popup_req, px, py);
    *push_in = TRUE;
}

static gchar * tooltip_for_file(FileName * file_cursor)
{
    setpwent ();
    struct passwd * pw_ent = getpwuid (file_cursor->stat_data.st_uid);
    gchar * s_user = g_strdup (pw_ent ? pw_ent->pw_name : "UNKNOWN");

    setgrent ();
    struct group * gw_ent = getgrgid (file_cursor->stat_data.st_gid);
    gchar * s_group = g_strdup (gw_ent ? gw_ent->gr_name : "UNKNOWN");

    gchar * tooltip = g_strdup_printf(_("%llu bytes, %s:%s %04o"),
        (unsigned long long)file_cursor->stat_data.st_size,
        s_user, s_group, (unsigned int)file_cursor->stat_data.st_mode);

    g_free(s_user);
    g_free(s_group);

    return tooltip;
}

/* Create a menu populated with all files and subdirectories. */
static GtkWidget * dirmenu_create_menu(Plugin * p, const char * path, gboolean open_at_top)
{
    DirMenuPlugin * dm = (DirMenuPlugin *) p->priv;

    /* Create a menu. */
    GtkWidget * menu = gtk_menu_new();

    if (dm->folder_icon == NULL)
    {
        int w;
        int h;
        gtk_icon_size_lookup_for_settings(gtk_widget_get_settings(menu), GTK_ICON_SIZE_MENU, &w, &h);
        dm->folder_icon = gtk_icon_theme_load_icon(
            gtk_icon_theme_get_default(),
            "gnome-fs-directory", MAX(w, h), 0, NULL);
        if (dm->folder_icon == NULL)
            dm->folder_icon = gtk_widget_render_icon(menu, GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU, NULL);
    }

    g_object_set_data_full(G_OBJECT(menu), "path", g_strdup(path), g_free);

    /* Scan the specified directory to populate the menu. */
    FileName * dir_list = NULL;
    FileName * file_list = NULL;
    int dir_list_count = 0;
    int file_list_count = 0;
    GDir * dir = g_dir_open(path, 0, NULL);
    if (dir != NULL)
    {
        const char * name;
        while ((name = g_dir_read_name(dir)) != NULL)	/* Memory owned by glib */
        {
            FileName ** plist = NULL;

            /* Omit hidden files. */
            if (name[0] == '.')
            {
                if (!dm->show_hidden || !strcmp(name, ".") || !strcmp(name, ".."))
                    continue;
            }

            char * full = g_build_filename(path, name, NULL);
            gboolean directory = g_file_test(full, G_FILE_TEST_IS_DIR);
            if (directory)
                plist = &dir_list,
                dir_list_count++;
            else if (dm->show_files)
                plist = &file_list,
                file_list_count++;

            if (plist)
            {
                FileName * list = *plist;

                /* Convert name to UTF-8 and to the collation key. */
                char * file_name = g_filename_display_name(name);
                char * file_name_collate_key = g_utf8_collate_key(file_name, -1);

                /* Allocate and initialize file name entry. */
                FileName * fn = g_new0(FileName, 1);
                fn->file_name = file_name;
                fn->file_name_collate_key = file_name_collate_key;
                fn->path = g_build_filename(path, file_name, NULL);
                fn->directory = directory;
                stat(fn->path, &fn->stat_data);

                int sort_by = directory ? dm->sort_directories : dm->sort_files;

                /* Locate insertion point. */
                FileName * fn_pred = NULL;
                FileName * fn_cursor;
                for (fn_cursor = list; fn_cursor != NULL; fn_pred = fn_cursor, fn_cursor = fn_cursor->flink)
                {
                    if (sort_by == SORT_BY_MTIME)
                    {
                        if (fn->stat_data.st_mtime > fn_cursor->stat_data.st_mtime)
                            break;
                    }
                    else if (sort_by == SORT_BY_SIZE)
                    {
                        if (fn->stat_data.st_size < fn_cursor->stat_data.st_size)
                            break;
                    }
                    else
                    {
                        if (strcmp(file_name_collate_key, fn_cursor->file_name_collate_key) <= 0)
                            break;
                    }
                }

                /* Insert file name entry into list. */
                if (fn_pred == NULL)
                {
                    fn->flink = list;
                    *plist = fn;
                }
                else
                {
                    fn->flink = fn_pred->flink;
                    fn_pred->flink = fn;
                }
            }
            g_free(full);
        }
        g_dir_close(dir);
    }

    gboolean not_empty_dir_list = dir_list != NULL;
    gboolean not_empty_file_list = file_list != NULL;
    gboolean not_empty = not_empty_dir_list || not_empty_file_list;

    /* The sorted directory name list is complete.  Loop to create the menu. */

    /* Subdirectories. */
    FileName * dir_cursor;
    while ((dir_cursor = dir_list) != NULL)
    {
        /* Create and initialize menu item. */
        GtkWidget * item = gtk_image_menu_item_new_with_label(dir_cursor->file_name);

/* FIXME: should we implement gtk_image_new_from_gicon to make show_icons option available on glib<2.20? */
#if GLIB_CHECK_VERSION(2,20,0)
        if (dm->show_icons)
        {
            GFile * file = g_file_new_for_path(dir_cursor->path);
            GFileInfo * file_info =g_file_query_info(file,
                G_FILE_ATTRIBUTE_STANDARD_ICON,
                G_FILE_QUERY_INFO_NONE,
                NULL,
                NULL);
            GIcon * icon = g_file_info_get_icon(file_info);
            if (icon)
            {
                GtkWidget * img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
                gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(item), img);
            }
            g_object_unref(G_OBJECT(file_info));
            g_object_unref(G_OBJECT(file));
        }
        else
#endif
        {
            gtk_image_menu_item_set_image(
                GTK_IMAGE_MENU_ITEM(item),
                gtk_image_new_from_stock(GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU));
        }

        GtkWidget * dummy = gtk_menu_new();
        if (!dm->plain_view)
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), dummy);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

        /* Unlink and free sorted directory name element, but reuse the directory name string. */
        dir_list = dir_cursor->flink;
        g_object_set_data_full(G_OBJECT(item), "name", dir_cursor->file_name, g_free);
        g_free(dir_cursor->file_name_collate_key);
        g_free(dir_cursor->path);
        g_free(dir_cursor);

        /* Connect signals. */
        if (dm->plain_view)
        {
            g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(dirmenu_menuitem_open_directory_plain), p);
        }
        else
        {
            g_signal_connect(G_OBJECT(item), "select", G_CALLBACK(dirmenu_menuitem_select), p);
            g_signal_connect(G_OBJECT(item), "deselect", G_CALLBACK(dirmenu_menuitem_deselect), p);
        }
    }

    if (not_empty_dir_list && not_empty_file_list)
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    /* File submenu. */
    GtkWidget * filemenu = menu;
    if (file_list_count > dm->max_file_count && not_empty_file_list)
    {
        GtkWidget * item = gtk_menu_item_new_with_mnemonic( _("Files") );
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(item));
        GtkWidget * submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
        filemenu = submenu;
    }

    /* Files. */
    FileName * file_cursor;
    while ((file_cursor = file_list) != NULL)
    {
        /* Create and initialize menu item. */
        GtkWidget * item = NULL;
        if (dm->show_file_size)
        {
            gchar * name = g_strdup_printf("%s [%llu]", file_cursor->file_name, (unsigned long long)file_cursor->stat_data.st_size);
            item = gtk_image_menu_item_new_with_label(name);
            g_free(name);
        }
        else
        {
            item = gtk_image_menu_item_new_with_label(file_cursor->file_name);
        }

        gchar * tooltip = tooltip_for_file(file_cursor);
        gtk_widget_set_tooltip_text(item, tooltip);
        g_free(tooltip);

        gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), item);

#if GLIB_CHECK_VERSION(2,20,0)
        if (dm->show_icons)
        {
            GFile * file = g_file_new_for_path(file_cursor->path);
            GFileInfo * file_info =g_file_query_info(file,
                G_FILE_ATTRIBUTE_STANDARD_ICON,
                G_FILE_QUERY_INFO_NONE,
                NULL,
                NULL);
            GIcon * icon = g_file_info_get_icon(file_info);
            if (icon)
            {
                GtkWidget * img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU);
                gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(item), img);
            }
            g_object_unref(G_OBJECT(file_info));
            g_object_unref(G_OBJECT(file));
        }
#endif
        /* Unlink and free file name element, but reuse the file path. */
        g_object_set_data_full(G_OBJECT(item), "path", file_cursor->path, g_free);
        file_list = file_cursor->flink;
        g_free(file_cursor->file_name);
        g_free(file_cursor->file_name_collate_key);
        g_free(file_cursor);

        /* Connect signals. */
        g_signal_connect(item, "activate", G_CALLBACK(dirmenu_menuitem_open_file), p);
    }

    if (!dm->plain_view)
    {
	/* Create "Open" and "Open in Terminal" items. */
	GtkWidget * item = gtk_image_menu_item_new_from_stock( GTK_STOCK_OPEN, NULL );
	g_signal_connect(item, "activate", G_CALLBACK(dirmenu_menuitem_open_directory), p);
	GtkWidget * term = gtk_menu_item_new_with_mnemonic( _("Open in _Terminal") );
	g_signal_connect(term, "activate", G_CALLBACK(dirmenu_menuitem_open_in_terminal), p);

	/* Insert or append based on caller's preference. */
	if (open_at_top)
	{
	    if (not_empty)
		gtk_menu_shell_insert(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new(), 0);
	    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), term, 0);
	    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, 0);
	}
	else {
	    if (not_empty)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
	    gtk_menu_shell_append(GTK_MENU_SHELL(menu), term);
	    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}
    }

    /* Show the menu and return. */
    gtk_widget_show_all(menu);
    return menu;
}

/* Show a menu of subdirectories. */
static void dirmenu_show_menu(GtkWidget * widget, Plugin * p, int btn, guint32 time)
{
    DirMenuPlugin * dm = (DirMenuPlugin *) p->priv;

    /* Create a menu populated with all subdirectories. */
    GtkWidget * menu = dirmenu_create_menu(
        p,
        ((dm->path != NULL) ? expand_tilda(dm->path) : g_get_home_dir()),
        FALSE);
    g_signal_connect(menu, "selection-done", G_CALLBACK(dirmenu_menu_selection_done), NULL);

    /* Show the menu.  Use a positioning function to get it placed next to the top level widget. */
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc) dirmenu_popup_set_position, p, btn, time);
}

/* Handler for button-press-event on top level widget. */
static gboolean dirmenu_button_press_event(GtkWidget * widget, GdkEventButton * event, Plugin * p)
{
    DirMenuPlugin * dm = (DirMenuPlugin *) p->priv;

    /* Standard left-click handling. */
    if (plugin_button_press_event(widget, event, p))
        return TRUE;

    if (event->button == 1)
    {
        dirmenu_show_menu(widget, p, event->button, event->time);
    }
    else
    {
        lxpanel_open_in_terminal( ((dm->path != NULL) ? expand_tilda(dm->path) : g_get_home_dir()) );
    }
    return TRUE;
}

/* Plugin constructor. */
static int dirmenu_constructor(Plugin * p, char ** fp)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    DirMenuPlugin * dm = g_new0(DirMenuPlugin, 1);
    dm->plugin = p;
    p->priv = dm;

    dm->show_hidden = FALSE;
    dm->show_files = TRUE;
    dm->max_file_count = 10;
    dm->show_file_size = FALSE;
    dm->show_icons = TRUE;
    dm->sort_directories = SORT_BY_NAME;
    dm->sort_files = SORT_BY_NAME;
    dm->plain_view = FALSE;

    /* Load parameters from the configuration file. */
    line s;
    s.len = 256;
    if (fp != NULL)
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END)
        {
            if (s.type == LINE_NONE)
            {
                ERR( "dirmenu: illegal token %s\n", s.str);
                return 0;
            }
            if (s.type == LINE_VAR)
            {
                if (g_ascii_strcasecmp(s.t[0], "image") == 0)
                    dm->image = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "path") == 0)
                    dm->path = g_strdup(s.t[1]);
		else if (g_ascii_strcasecmp(s.t[0], "name") == 0)
                    dm->name = g_strdup( s.t[1] );
                else if (g_ascii_strcasecmp(s.t[0], "ShowHidden") == 0)
                    dm->show_hidden = str2num(bool_pair, s.t[1], dm->show_hidden);
                else if (g_ascii_strcasecmp(s.t[0], "ShowFiles") == 0)
                    dm->show_files = str2num(bool_pair, s.t[1], dm->show_files);
                else if (g_ascii_strcasecmp(s.t[0], "MaxFileCount") == 0)
                    dm->max_file_count = atoi(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "ShowFileSize") == 0)
                    dm->show_file_size = str2num(bool_pair, s.t[1], dm->show_file_size);
                else if (g_ascii_strcasecmp(s.t[0], "ShowIcons") == 0)
                    dm->show_icons = str2num(bool_pair, s.t[1], dm->show_icons);
                else if (g_ascii_strcasecmp(s.t[0], "SortDirectoriesBy") == 0)
                    dm->sort_directories = str2num(sort_by_pair, s.t[1], dm->sort_directories);
                else if (g_ascii_strcasecmp(s.t[0], "SortFilesBy") == 0)
                    dm->sort_files = str2num(sort_by_pair, s.t[1], dm->sort_files);
                else if (g_ascii_strcasecmp(s.t[0], "PlainView") == 0)
                    dm->plain_view = str2num(bool_pair, s.t[1], dm->plain_view);
                else
                    ERR( "dirmenu: unknown var %s\n", s.t[0]);
            }
            else
            {
                ERR( "dirmenu: illegal in this context %s\n", s.str);
                return 0;
            }
        }
    }

    /* Allocate top level widget and set into Plugin widget pointer.
     * It is not known why, but the button text will not draw if it is edited from empty to non-empty
     * unless this strategy of initializing it with a non-empty value first is followed. */
    p->pwid = fb_button_new_from_file_with_label(
        ((dm->image != NULL) ? dm->image : "file-manager"),
        p->panel->icon_size, p->panel->icon_size, PANEL_ICON_HIGHLIGHT, TRUE, p->panel, "Temp");
    gtk_container_set_border_width(GTK_CONTAINER(p->pwid), 0);
    g_signal_connect(p->pwid, "button_press_event", G_CALLBACK(dirmenu_button_press_event), p);

    /* Initialize the widget. */
    dirmenu_apply_configuration(p);

    /* Show the widget and return. */
    gtk_widget_show(p->pwid);
    return 1;
}

/* Plugin destructor. */
static void dirmenu_destructor(Plugin * p)
{
    DirMenuPlugin * dm = (DirMenuPlugin *) p->priv;

    /* Release a reference on the folder icon if held. */
    if (dm->folder_icon)
        g_object_unref(dm->folder_icon);

    /* Deallocate all memory. */
    g_free(dm->image);
    g_free(dm->path);
    g_free(dm->name);
    g_free(dm);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static void dirmenu_apply_configuration(Plugin * p)
{
    DirMenuPlugin * dm = (DirMenuPlugin *) p->priv;

    gchar * icon_name = NULL;
    
#if GLIB_CHECK_VERSION(2,20,0)
    if (!dm->image)
    {
	GFile * file = g_file_new_for_path( ((dm->path != NULL) ? expand_tilda(dm->path) : g_get_home_dir()) );
	GFileInfo * file_info =g_file_query_info(file,
	    G_FILE_ATTRIBUTE_STANDARD_ICON,
	    G_FILE_QUERY_INFO_NONE,
	    NULL,
	    NULL);
	GIcon * icon = g_file_info_get_icon(file_info);
	if (icon)
	{
	    gchar * name = g_icon_to_string(icon);
	    icon_name = g_strdup_printf("GIcon %s", name);
	    g_free(name);
	}
	g_object_unref(G_OBJECT(file_info));
	g_object_unref(G_OBJECT(file));
    }
#endif


    fb_button_set_from_file(p->pwid,
        ((dm->image != NULL) ? dm->image : (icon_name != NULL) ? icon_name : "file-manager"),
        ((dm->image != NULL) ? -1 : p->panel->icon_size), p->panel->icon_size, TRUE);
    fb_button_set_label(p->pwid, p->panel, dm->name);
    gtk_widget_set_tooltip_text(p->pwid, ((dm->path != NULL) ? expand_tilda(dm->path) : g_get_home_dir()));
    fb_button_set_orientation(p->pwid, p->panel->orientation);

    g_free(icon_name);
}

/* Callback when the configuration dialog is to be shown. */
static void dirmenu_configure(Plugin * p, GtkWindow * parent)
{
    const char* sort_by = _("|Name|Modification time (descending)|Size");
    char* sort_directories = g_strdup_printf("%s%s", _("|Sort directories by"), sort_by);
    char* sort_files = g_strdup_printf("%s%s", _("|Sort files by"), sort_by);

    DirMenuPlugin * dm = (DirMenuPlugin *) p->priv;
    GtkWidget * dlg = create_generic_config_dlg(
        _(p->class->name),
        GTK_WIDGET(parent),
        (GSourceFunc) dirmenu_apply_configuration, (gpointer) p,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("Directory"), &dm->path, (GType)CONF_TYPE_DIRECTORY_ENTRY,
        _("Label"), &dm->name, (GType)CONF_TYPE_STR,
        _("Icon"), &dm->image, (GType)CONF_TYPE_FILE_ENTRY,
        "", 0, (GType)CONF_TYPE_END_TABLE,
        _("Plain view"), &dm->plain_view, (GType)CONF_TYPE_BOOL,
        _("Show hidden files or folders"), &dm->show_hidden, (GType)CONF_TYPE_BOOL,
        _("Show files"), &dm->show_files, (GType)CONF_TYPE_BOOL,
        _("Use submenu if number of files is more than"), &dm->max_file_count, (GType)CONF_TYPE_INT,
        _("Show file size"), &dm->show_file_size, (GType)CONF_TYPE_BOOL,
        _("Show MIME type icons"), &dm->show_icons, (GType)CONF_TYPE_BOOL,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        sort_directories, (gpointer)&dm->sort_directories, (GType)CONF_TYPE_ENUM,
        sort_files, (gpointer)&dm->sort_files, (GType)CONF_TYPE_ENUM,
        "", 0, (GType)CONF_TYPE_END_TABLE,
        NULL);

    if (dlg)
        gtk_window_present(GTK_WINDOW(dlg));

    g_free(sort_directories);
    g_free(sort_files);
}

/* Callback when the configuration is to be saved. */
static void dirmenu_save_configuration(Plugin * p, FILE * fp)
{
    DirMenuPlugin * dm = (DirMenuPlugin *) p->priv;
    lxpanel_put_str(fp, "path", dm->path);
    lxpanel_put_str(fp, "name", dm->name);
    lxpanel_put_str(fp, "image", dm->image);
    lxpanel_put_bool(fp, "ShowHidden", dm->show_hidden);
    lxpanel_put_bool(fp, "ShowFiles", dm->show_files);
    lxpanel_put_int(fp, "MaxFileCount", dm->max_file_count);
    lxpanel_put_bool(fp, "ShowFileSize", dm->show_file_size);
    lxpanel_put_bool(fp, "ShowIcons", dm->show_icons);
    lxpanel_put_enum(fp, "SortDirectoriesBy", dm->sort_directories, sort_by_pair);
    lxpanel_put_enum(fp, "SortFilesBy", dm->sort_files, sort_by_pair);
    lxpanel_put_bool(fp, "PlainView", dm->plain_view);
}

/* Callback when panel configuration changes. */
static void dirmenu_panel_configuration_changed(Plugin * p)
{
    DirMenuPlugin * dm = (DirMenuPlugin *) p->priv;
    fb_button_set_from_file(p->pwid,
        ((dm->image != NULL) ? dm->image : "file-manager"),
        p->panel->icon_size, p->panel->icon_size, TRUE);
    dirmenu_apply_configuration(p);
}

/* Plugin descriptor. */
PluginClass dirmenu_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "dirmenu",
    name : N_("Directory Menu"),
    version: "1.0",
    description : N_("Browse directory tree via menu (Author: PCMan)"),

    constructor : dirmenu_constructor,
    destructor  : dirmenu_destructor,
    config : dirmenu_configure,
    save : dirmenu_save_configuration,
    panel_configuration_changed : dirmenu_panel_configuration_changed

};
