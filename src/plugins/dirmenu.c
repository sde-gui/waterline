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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <gio/gio.h>

#define PLUGIN_PRIV_TYPE DirMenuPlugin

#include <lxpanelx/panel.h>
#include <lxpanelx/misc.h>
#include <lxpanelx/plugin.h>
#include <lxpanelx/libfm.h>
#include <lxpanelx/dbg.h>

#include <lxpanelx/gtkcompat.h>

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
    gboolean show_tooltips;
} DirMenuPlugin;

static void dirmenu_menuitem_open_file(GtkWidget * item, Plugin * p);
static void dirmenu_menuitem_open_directory(GtkWidget * item, Plugin * p);
static void dirmenu_menuitem_open_in_terminal(GtkWidget * item, Plugin * p);
static void dirmenu_menuitem_select(GtkMenuItem * item, Plugin * p);
static void dirmenu_menuitem_deselect(GtkMenuItem * item, Plugin * p);
void dirmenu_menu_selection_done(GtkWidget * menu, Plugin * p);
static void dirmenu_popup_set_position(GtkWidget * menu, gint * px, gint * py, gboolean * push_in, Plugin * p);
static GtkWidget * dirmenu_create_menu(Plugin * p, const char * path, gboolean open_at_top, GtkWidget * parent_item);
static void dirmenu_show_menu(Plugin * p, int btn, guint32 time);
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

static gboolean dirmenu_menuitem_button_press(GtkWidget * item, GdkEventButton* evt, Plugin * p)
{
    //DirMenuPlugin * dm = PRIV(p);

    if (evt->button == 3)  /* right */
    {
        /*if (lxpanel_is_in_kiosk_mode())
            return TRUE;*/

        gchar * path = g_object_get_data(G_OBJECT(item), "path");
        if (path)
        {
            path = g_strdup(path);
        }
        else
        {
            GtkMenu * parent = GTK_MENU(gtk_widget_get_parent(GTK_WIDGET(item)));
            if (!parent)
                goto out;
            gchar * parent_path = (gchar *) g_object_get_data(G_OBJECT(parent), "path");
            if (!parent_path)
                goto out;
            gchar * name = (gchar *) g_object_get_data(G_OBJECT(item), "name");
            if (!name)
                goto out;
            path = g_build_filename(parent_path, name, NULL);
            if (!path)
                goto out;
        }

        GtkMenu * popup = lxpanel_fm_file_menu_for_path(path);

        if (popup)
        {
            g_signal_connect(popup, "deactivate", G_CALLBACK(restore_grabs), item);
            gtk_menu_popup(popup, NULL, NULL, NULL, NULL, 3, evt->time);
        }

        out:

        if (path)
            g_free(path);

        return TRUE;
    }
    return FALSE;
}

static gboolean dirmenu_menuitem_button_release(GtkWidget * item, GdkEventButton* evt, Plugin * p)
{
    if( evt->button == 3)
    {
        return TRUE;
    }

    return FALSE;
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
            sub = dirmenu_create_menu(p, path, TRUE, GTK_WIDGET(item));
            g_free(path);
            gtk_menu_item_set_submenu(item, sub);
        }
    }
}

static gboolean dirmenu_menuitem_timeout(gpointer user_data)
{
    GtkWidget * sub = GTK_WIDGET(user_data);
    if (!sub)
        return FALSE;

    if (!gtk_widget_get_visible(sub))
    {
        GtkMenuItem * item = (GtkMenuItem *) g_object_get_data(G_OBJECT(sub), "parent_item");
        if (item)
        {
            gtk_menu_item_set_submenu(item, gtk_menu_new());
        }
    }

    return TRUE;
}

static void dirmenu_menuitem_weak_ref(gpointer data, GObject *where_the_object_was)
{
    g_source_remove((guint)data);
}

/* Handler for deselect event on popup menu item. */
static void dirmenu_menuitem_deselect(GtkMenuItem * item, Plugin * p)
{
    /* Delete old menu on deselect to save resource. */
//    gtk_menu_item_set_submenu(item, gtk_menu_new());

    GtkWidget * sub = gtk_menu_item_get_submenu(item);
    if (sub == NULL)
         return;

    char * path = (char *) g_object_get_data(G_OBJECT(sub), "path");
    if (path == NULL)
         return;

    guint timer_id = (guint) g_object_get_data(G_OBJECT(sub), "timer_id");
    if (!timer_id)
    {
        timer_id = g_timeout_add(60000, dirmenu_menuitem_timeout, sub);
        g_object_set_data(G_OBJECT(sub), "timer_id", (gpointer)timer_id);
        g_object_weak_ref(G_OBJECT(sub), dirmenu_menuitem_weak_ref, (gpointer)timer_id);

        g_object_set_data(G_OBJECT(sub), "parent_item", (gpointer)item);
    }
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
    plugin_popup_set_position_helper(p, plugin_widget(p), menu, &popup_req, px, py);
    *push_in = TRUE;
}

static gboolean dirmenu_query_tooltip(GtkWidget * item, gint x, gint y, gboolean keyboard_mode,
                                      GtkTooltip * tooltip, Plugin * p)
{
    g_signal_handlers_disconnect_by_func(G_OBJECT(item), dirmenu_query_tooltip, p);

    gchar * path = (gchar * ) g_object_get_data(G_OBJECT(item), "path");
//g_print("%s\n", path);
    if (!path)
        return FALSE;

    int link_content_size = 0;
    gchar link_content[1024];
    gboolean is_symlink = g_file_test(path, G_FILE_TEST_IS_SYMLINK);
    if (is_symlink)
    {
        link_content_size = readlink(path, link_content, 1023);
        if (link_content_size >= 0)
            link_content[link_content_size] = 0;
    }

    struct stat stat_data;
    if (stat(path, &stat_data) !=0)
        return FALSE;

    gchar * tooltip_text = lxpanel_tooltip_for_file_stat(&stat_data);

    if (link_content_size > 0)
    {
        gchar * s = g_strdup_printf(_("Link to %s,\n%s"), link_content, tooltip_text);
        g_free(tooltip_text);
        tooltip_text = s;
    }

    gtk_tooltip_set_text(tooltip, tooltip_text);
    gtk_widget_set_tooltip_text(item, tooltip_text);

    g_free(tooltip_text);

    //gtk_widget_set_has_tooltip(item, TRUE);

    return TRUE;
}

/* Create a menu populated with all files and subdirectories. */
static GtkWidget * dirmenu_create_menu(Plugin * p, const char * path, gboolean open_at_top, GtkWidget * parent_item)
{
    DirMenuPlugin * dm = PRIV(p);

//g_print("[%d] %s\n", (int)time(NULL), path);

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
    int hidden_count = 0;
    unsigned long long total_file_size = 0;
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
                if (!strcmp(name, ".") || !strcmp(name, ".."))
                    continue;

                if (!dm->show_hidden)
                {
                    hidden_count++;
                    continue;
                }
            }

            /* Full path. */
            char * item_path = g_build_filename(path, name, NULL);

            /* Stat */
            struct stat stat_data;
            gboolean directory;
            if (stat(item_path, &stat_data) == 0)
                directory = S_ISDIR(stat_data.st_mode);
            else
                directory = FALSE;

            int sort_by;

            /* Choose list */
            if (directory)
                plist = &dir_list,
                sort_by = dm->sort_directories,
                dir_list_count++;
            else if (dm->show_files)
                plist = &file_list,
                sort_by = dm->sort_files,
                file_list_count++,
                total_file_size += stat_data.st_size;

            if (plist)
            {
                FileName * list = *plist;

                /* Allocate and initialize file name entry. */
                FileName * fn = g_new0(FileName, 1);
                fn->file_name = g_filename_display_name(name);
                char * file_name_collate_key = fn->file_name_collate_key =
                    (sort_by == SORT_BY_NAME) ? g_utf8_collate_key(fn->file_name, -1) : NULL;
                fn->path = item_path;
                fn->directory = directory;
                memcpy(&fn->stat_data, &stat_data, sizeof(struct stat));

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
                    else if (sort_by == SORT_BY_NAME)
                    {
                        if (strcmp(file_name_collate_key, fn_cursor->file_name_collate_key) <= 0)
                            break;
                    }
                    else
                    {
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
        }
        g_dir_close(dir);
    }


    gboolean not_empty_dir_list = dir_list != NULL;
    gboolean not_empty_file_list = file_list != NULL;
    gboolean not_empty = not_empty_dir_list || not_empty_file_list;

    /* The sorted directory name list is complete.  Loop to create the menu. */

//g_print("[%d] subdirectories...\n", (int)time(NULL));

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
            GFileInfo * file_info = g_file_query_info(file,
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
        g_signal_connect(item, "button-press-event", G_CALLBACK(dirmenu_menuitem_button_press), p);
        g_signal_connect(item, "button-release-event", G_CALLBACK(dirmenu_menuitem_button_release), p);
    }

    if (not_empty_dir_list && not_empty_file_list)
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

//g_print("[%d] files...\n", (int)time(NULL));

    /* File submenu. */
    GtkWidget * filemenu = menu;
    if (file_list_count > dm->max_file_count && not_empty_file_list)
    {
        gchar * filemenu_title = g_strdup_printf(_("Files (%d)"), file_list_count);
        GtkWidget * item = gtk_menu_item_new_with_mnemonic( filemenu_title );
        g_free(filemenu_title);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(item));

        GtkWidget * submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

        filemenu = submenu;
    }

    int  submenu_index_len = 0;
    char submenu_index[20] = {0};
    int  submenu_item_count = 0;
    GtkWidget * filesubmenu = NULL;

    /* Files. */
    FileName * file_cursor;
    while ((file_cursor = file_list) != NULL)
    {
        /* Create and initialize menu item. */
        GtkWidget * item = NULL;
        if (dm->show_file_size)
        {
            gchar * name = g_strdup_printf("[%'llu] %s", (unsigned long long)file_cursor->stat_data.st_size, file_cursor->file_name);
            item = gtk_image_menu_item_new_with_label(name);
            g_free(name);
        }
        else
        {
            item = gtk_image_menu_item_new_with_label(file_cursor->file_name);
        }

        if (dm->show_tooltips)
        {
            /*gulong query_tooltip_handler_id =*/ g_signal_connect(G_OBJECT(item), "query-tooltip", G_CALLBACK(dirmenu_query_tooltip), p);
            //g_object_set_data(G_OBJECT(item), "query_tooltip_handler_id", (gpointer)query_tooltip_handler_id);
            gtk_widget_set_has_tooltip(item, TRUE);
        }

        GtkWidget * add_to_menu = NULL;
        if (dm->sort_files == SORT_BY_NAME && file_list_count > 100 && file_list_count > dm->max_file_count)
        {
            if (!filesubmenu || submenu_item_count < 1)
            {
                gchar * nc = g_utf8_next_char(file_cursor->file_name_collate_key);
                submenu_index_len = nc - file_cursor->file_name_collate_key;
                memcpy(submenu_index, file_cursor->file_name_collate_key, submenu_index_len);
                submenu_index[submenu_index_len] = 0;

                FileName * file_cursor2 = file_cursor->flink;
                submenu_item_count = 1;
                while (file_cursor2 && memcmp(submenu_index, file_cursor2->file_name_collate_key, submenu_index_len) == 0)
                {
                    submenu_item_count++;
                    file_cursor2 = file_cursor2->flink;
                }

                if (submenu_item_count > 2)
                {
                    gchar * nc = g_utf8_next_char(file_cursor->file_name);
                    gchar * submenu_index_name = g_utf8_strup(file_cursor->file_name, nc - file_cursor->file_name);
                    gchar * submenu_index_title = g_strdup_printf(_("%s (%d)"), submenu_index_name, submenu_item_count);

                    GtkWidget * submenu_item = gtk_menu_item_new_with_label( submenu_index_title );
                    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), GTK_WIDGET(submenu_item));
                    filesubmenu = gtk_menu_new();
                    gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu_item), filesubmenu);

                    g_free(submenu_index_title);
                    g_free(submenu_index_name);

                    add_to_menu = filesubmenu;
                }
                else
                {
                    filesubmenu = NULL;
                    add_to_menu = filemenu;
                }
            }
            else
            {
                add_to_menu = filesubmenu;
            }
            submenu_item_count--;
        }
        else
        {
            add_to_menu = filemenu;
        }

        gtk_menu_shell_append(GTK_MENU_SHELL(add_to_menu), item);


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
        g_signal_connect(item, "button-press-event", G_CALLBACK(dirmenu_menuitem_button_press), p);
        g_signal_connect(item, "button-release-event", G_CALLBACK(dirmenu_menuitem_button_release), p);
    }

//g_print("[%d] done\n", (int)time(NULL));

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

    /* Show the menu. */
    gtk_widget_show_all(menu);


    if (parent_item && dm->show_tooltips)
    {
        gchar * s1 = NULL;
        gchar * s2 = NULL;

        struct stat stat_data;
        stat(path, &stat_data);
        s1 = lxpanel_tooltip_for_file_stat(&stat_data);

        int link_content_size = 0;
        gchar link_content[1024];
        gboolean is_symlink = g_file_test(path, G_FILE_TEST_IS_SYMLINK);
        if (is_symlink)
        {
            link_content_size = readlink(path, link_content, 1023);
            if (link_content_size >= 0)
                link_content[link_content_size] = 0;
        }

        if (link_content_size > 0)
        {
            s2 = g_strdup_printf(_("Link to %s,\n%s"), link_content, s1);
            g_free(s1);
            s1 = s2;
        }

        if (dir_list_count)
        {
            gchar * s3 = s1 ? s1 : "";
            gchar * s4 = s1 ? _(",\n") : "";
            s2 = g_strdup_printf(_("%s%s%d subdirectories"), s3, s4, dir_list_count);
            g_free(s1);
            s1 = s2;
        }

        if (file_list_count)
        {
            gchar * s3 = s1 ? s1 : "";
            gchar * s4 = s1 ? _(",\n") : "";
            s2 = g_strdup_printf(_("%s%s%d files containing %'llu bytes"), s3, s4, file_list_count, total_file_size);
            g_free(s1);
            s1 = s2;
        }

        if (hidden_count)
        {
            gchar * s3 = s1 ? s1 : "";
            gchar * s4 = s1 ? _(",\n") : "";
            s2 = g_strdup_printf(_("%s%s%d hidden items"), s3, s4, hidden_count);
            g_free(s1);
            s1 = s2;
        }

        gtk_widget_set_tooltip_text(parent_item, s1);
        g_free(s1);
    }


    return menu;
}

/* Show a menu of subdirectories. */
static void dirmenu_show_menu(Plugin * p, int btn, guint32 time)
{
    DirMenuPlugin * dm = PRIV(p);

    /* Create a menu populated with all subdirectories. */
    GtkWidget * menu = dirmenu_create_menu(
        p,
        ((dm->path != NULL) ? expand_tilda(dm->path) : g_get_home_dir()),
        FALSE, NULL);
    g_signal_connect(menu, "selection-done", G_CALLBACK(dirmenu_menu_selection_done), NULL);

    /* Show the menu.  Use a positioning function to get it placed next to the top level widget. */
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc) dirmenu_popup_set_position, p, btn, time);
}

/* Handler for button-press-event on top level widget. */
static gboolean dirmenu_button_press_event(GtkWidget * widget, GdkEventButton * event, Plugin * p)
{
    DirMenuPlugin * dm = PRIV(p);

    /* Standard left-click handling. */
    if (plugin_button_press_event(widget, event, p))
        return TRUE;

    if (event->button == 1)
    {
        dirmenu_show_menu(p, event->button, event->time);
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
    plugin_set_priv(p, dm);

    dm->show_hidden = FALSE;
    dm->show_files = TRUE;
    dm->max_file_count = 10;
    dm->show_file_size = FALSE;
    dm->show_icons = TRUE;
    dm->show_tooltips = TRUE;
    dm->sort_directories = SORT_BY_NAME;
    dm->sort_files = SORT_BY_NAME;
    dm->plain_view = FALSE;

    /* Load parameters from the configuration file. */
    line s;
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
                else if (g_ascii_strcasecmp(s.t[0], "ShowTooltips") == 0)
                    dm->show_tooltips = str2num(bool_pair, s.t[1], dm->show_tooltips);
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
    GtkWidget * pwid = fb_button_new_from_file_with_label(
        ((dm->image != NULL) ? dm->image : "file-manager"),
        plugin_get_icon_size(p), plugin_get_icon_size(p), TRUE, TRUE, plugin_panel(p), "Temp");
    plugin_set_widget(p, pwid);
    gtk_container_set_border_width(GTK_CONTAINER(pwid), 0);
    g_signal_connect(pwid, "button_press_event", G_CALLBACK(dirmenu_button_press_event), p);

    /* Initialize the widget. */
    dirmenu_apply_configuration(p);

    /* Show the widget and return. */
    gtk_widget_show(pwid);
    return 1;
}

/* Plugin destructor. */
static void dirmenu_destructor(Plugin * p)
{
    DirMenuPlugin * dm = PRIV(p);

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
    DirMenuPlugin * dm = PRIV(p);

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


    fb_button_set_from_file(plugin_widget(p),
        ((dm->image != NULL) ? dm->image : (icon_name != NULL) ? icon_name : "file-manager"),
        ((dm->image != NULL) ? -1 : plugin_get_icon_size(p)), plugin_get_icon_size(p), TRUE);
    fb_button_set_label(plugin_widget(p), plugin_panel(p), dm->name);
    gtk_widget_set_tooltip_text(plugin_widget(p), ((dm->path != NULL) ? expand_tilda(dm->path) : g_get_home_dir()));
    fb_button_set_orientation(plugin_widget(p), plugin_get_orientation(p));

    g_free(icon_name);
}

/* Callback when the configuration dialog is to be shown. */
static void dirmenu_configure(Plugin * p, GtkWindow * parent)
{
    const char* sort_by = _("|Name|Modification time (descending)|Size");
    char* sort_directories = g_strdup_printf("%s%s", _("|Sort directories by"), sort_by);
    char* sort_files = g_strdup_printf("%s%s", _("|Sort files by"), sort_by);

    DirMenuPlugin * dm = PRIV(p);
    GtkWidget * dlg = create_generic_config_dlg(
        _(plugin_class(p)->name),
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
        _("Show tooltips"), &dm->show_tooltips, (GType)CONF_TYPE_BOOL,
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
    DirMenuPlugin * dm = PRIV(p);
    lxpanel_put_str(fp, "path", dm->path);
    lxpanel_put_str(fp, "name", dm->name);
    lxpanel_put_str(fp, "image", dm->image);
    lxpanel_put_bool(fp, "ShowHidden", dm->show_hidden);
    lxpanel_put_bool(fp, "ShowFiles", dm->show_files);
    lxpanel_put_int(fp, "MaxFileCount", dm->max_file_count);
    lxpanel_put_bool(fp, "ShowFileSize", dm->show_file_size);
    lxpanel_put_bool(fp, "ShowIcons", dm->show_icons);
    lxpanel_put_bool(fp, "ShowTooltips", dm->show_tooltips);
    lxpanel_put_enum(fp, "SortDirectoriesBy", dm->sort_directories, sort_by_pair);
    lxpanel_put_enum(fp, "SortFilesBy", dm->sort_files, sort_by_pair);
    lxpanel_put_bool(fp, "PlainView", dm->plain_view);
}

/* Callback when panel configuration changes. */
static void dirmenu_panel_configuration_changed(Plugin * p)
{
    DirMenuPlugin * dm = PRIV(p);
    fb_button_set_from_file(plugin_widget(p),
        ((dm->image != NULL) ? dm->image : "file-manager"),
        plugin_get_icon_size(p), plugin_get_icon_size(p), TRUE);
    dirmenu_apply_configuration(p);
}


static void dirmenu_run_command_activate(Plugin * p, char ** argv, int argc)
{
    dirmenu_show_menu(p, 0, 0);
}

static void dirmenu_run_command(Plugin * p, char ** argv, int argc)
{
    if (argc < 1)
        return;

    if (strcmp(argv[0], "activate") == 0)
    {
        dirmenu_run_command_activate(p, argv + 1, argc - 1);
    }
}

static void dirmenu_popup_menu_hook(struct _Plugin * plugin, GtkMenu * menu)
{
    DirMenuPlugin * dm = PRIV(plugin);

    const gchar * path = (dm->path != NULL) ? expand_tilda(dm->path) : g_get_home_dir();
    GtkMenu * file_menu = lxpanel_fm_file_menu_for_path(path);
    if (file_menu)
    {
        GtkWidget * item = gtk_separator_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);

        item = gtk_image_menu_item_new_with_label(path);
        gtk_menu_item_set_submenu(item, file_menu);
        gtk_widget_show(item);
        gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);
    }
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
    panel_configuration_changed : dirmenu_panel_configuration_changed,
    run_command : dirmenu_run_command,
    popup_menu_hook : dirmenu_popup_menu_hook
};
