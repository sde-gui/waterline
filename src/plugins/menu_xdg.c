GQuark SYS_MENU_ITEM_ID = 0;
GQuark SYS_MENU_ITEM_MANAGED_ICON_ID = 0;
GQuark SYS_MENU_HEAD_ITEM_ID = 0;

static gboolean sys_menu_item_has_data(gpointer item)
{
   return (g_object_get_qdata(G_OBJECT(item), SYS_MENU_ITEM_ID) != NULL);
}

static gboolean sys_menu_item_has_managed_icon(gpointer item)
{
   return (g_object_get_qdata(G_OBJECT(item), SYS_MENU_ITEM_MANAGED_ICON_ID) != NULL);
}

static gboolean sys_menu_item_is_head(gpointer item)
{
   return (g_object_get_qdata(G_OBJECT(item), SYS_MENU_HEAD_ITEM_ID) != NULL);
}

/********************************************************************/

static void on_menu_item( GtkMenuItem* mi, MenuCacheItem* item )
{
    wtl_launch_app( menu_cache_app_get_exec(MENU_CACHE_APP(item)),
            NULL, menu_cache_app_get_use_terminal(MENU_CACHE_APP(item)));
}


/* load icon when mapping the menu item to speed up */
static void on_menu_item_map(GtkWidget* mi, gpointer * _user_data)
{
    if (!sys_menu_item_has_managed_icon(mi))
        return;

    GtkImage* img = GTK_IMAGE(gtk_image_menu_item_get_image(GTK_IMAGE_MENU_ITEM(mi)));
    if (img)
    {
        if (gtk_image_get_storage_type(img) == GTK_IMAGE_EMPTY)
        {
            int w, h;
            gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
            MenuCacheItem * item = g_object_get_qdata(G_OBJECT(mi), SYS_MENU_ITEM_ID);
            GdkPixbuf * icon = wtl_load_icon(menu_cache_item_get_icon(item), w, h, TRUE);
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
    on_menu_item_map(GTK_WIDGET(mi), NULL);
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

/********************************************************************/

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

/********************************************************************/

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

static gboolean _is_icon_name_valid_for_gtk(const char * icon_name)
{
    while (*icon_name)
    {
        char c = *icon_name;
        gboolean valid = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '-');
        if (!valid)
            return FALSE;
        icon_name++;
    }

    return TRUE;
}

static GtkWidget* create_item( MenuCacheItem* item )
{
    GtkWidget* mi;
    if( menu_cache_item_get_type(item) == MENU_CACHE_TYPE_SEP )
        mi = gtk_separator_menu_item_new();
    else
    {
        const char * name = menu_cache_item_get_name(item);
        su_log_debug2("Name    = %s", name);
        if (!name)
            name = "<unknown>";

        mi = gtk_image_menu_item_new_with_label(name);

        const char * icon_name = menu_cache_item_get_icon(item);
        su_log_debug2("Icon    = %s", icon_name);
        if (su_str_empty(icon_name))
            icon_name = "applications-other";
        GtkWidget * image = NULL;
        if (_is_icon_name_valid_for_gtk(icon_name))
        {
            image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
        }
        else
        {
            g_object_set_qdata_full(G_OBJECT(mi),
                SYS_MENU_ITEM_MANAGED_ICON_ID, GINT_TO_POINTER(1), NULL);
            image = gtk_image_new();
        }
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), image);

        if( menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP )
        {
            const gchar * tooltip = menu_cache_item_get_comment(item);
            su_log_debug2("Tooltip = %s", tooltip);

/*
            FIXME: to be implemented in menu-cache
            if (su_str_empty(tooltip))
                tooltip = menu_cache_item_get_generic_name(item);
*/
            gchar * additional_tooltip = NULL;

            const gchar * commandline = menu_cache_app_get_exec(MENU_CACHE_APP(item));
            su_log_debug2("Exec    = %s", commandline);

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
        g_signal_connect(mi, "map", G_CALLBACK(on_menu_item_map), NULL);
        g_signal_connect(mi, "style-set", G_CALLBACK(on_menu_item_style_set), item);
        g_signal_connect(mi, "button-press-event", G_CALLBACK(on_menu_button_press), item);
        g_signal_connect(mi, "button-release-event", G_CALLBACK(on_menu_button_release), item);
    }
    gtk_widget_show( mi );
    g_object_set_qdata_full( G_OBJECT(mi), SYS_MENU_ITEM_ID, menu_cache_item_ref(item), (GDestroyNotify) menu_cache_item_unref );
    return mi;
}

/********************************************************************/

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


static void unload_old_icons(GtkMenu* menu, GtkIconTheme* theme)
{
    GList * child;
    GList * children = gtk_container_get_children(GTK_CONTAINER(menu));
    for (child = children; child; child = child->next)
    {
        GtkMenuItem * item = GTK_MENU_ITEM(child->data);

        GtkWidget * sub_menu = gtk_menu_item_get_submenu(item);
        if (sub_menu)
            unload_old_icons(GTK_MENU(sub_menu), theme);

        if (sys_menu_item_has_data(item) && sys_menu_item_has_managed_icon(item) && GTK_IS_IMAGE_MENU_ITEM(item))
        {
            GtkImage * image = GTK_IMAGE(gtk_image_menu_item_get_image(GTK_IMAGE_MENU_ITEM(item)));
            gtk_image_clear(image);
            if (gtk_widget_get_mapped(GTK_WIDGET(image)))
                on_menu_item_map(GTK_WIDGET(item), NULL);
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
 * position: Position to insert items.
             Passing -1 in this parameter means append all items
             at the end of menu.
 */
static void sys_menu_insert_items( menup* m, GtkMenu* menu, int position )
{
    MenuCacheDir* dir;
    guint change_handler;

    dir = menu_cache_get_root_dir(m->menu_cache);
    if (dir)
        load_menu(m, dir, GTK_WIDGET(menu), position);
    else
        su_log_debug("menu_cache_get_root_dir() returned NULL");

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

    children = gtk_container_get_children( GTK_CONTAINER(menu) );
    for (child = children, idx = 0; child; child = child->next, ++idx)
    {
        item = GTK_MENU_ITEM( child->data );
        if (sys_menu_item_is_head(item))
        {
            child = child->next;
            while (child && sys_menu_item_has_data(child->data))
            {
                item = GTK_MENU_ITEM(child->data);
                child = child->next;
                gtk_widget_destroy(GTK_WIDGET(item));
            }
            sys_menu_insert_items(m, menu, idx + 1);
            if (!child)
                break;
        }
        else if( ( sub_menu = gtk_menu_item_get_submenu( item ) ) )
        {
            reload_system_menu( m, GTK_MENU(sub_menu) );
        }
    }
    g_list_free( children );
}


static gboolean on_timeout_reload_menu(menup * m)
{
    m->menu_reload_timeout_cb = 0;
    reload_system_menu(m, GTK_MENU(m->menu));
    return FALSE;
}

static void on_reload_menu(MenuCache * cache, menup * m)
{
    su_log_debug("got menu reload notification");

    if (!m->menu_reload_timeout_cb)
    {
        m->menu_reload_timeout_cb = g_timeout_add(2000, (GSourceFunc) on_timeout_reload_menu, m);
    }
}

static void
read_system_menu(GtkMenu* menu, Plugin *p, json_t * json_item)
{
    menup *m = PRIV(p);

    if (SYS_MENU_ITEM_ID == 0)
        SYS_MENU_ITEM_ID = g_quark_from_static_string("SysMenuItem");
    if (SYS_MENU_ITEM_MANAGED_ICON_ID == 0)
        SYS_MENU_ITEM_MANAGED_ICON_ID = g_quark_from_static_string("SysMenuItemManagedIcon");
    if (SYS_MENU_HEAD_ITEM_ID == 0)
        SYS_MENU_HEAD_ITEM_ID = g_quark_from_static_string("SysMenuHeadItem");

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

    GtkWidget* mi = gtk_menu_item_new();
    g_object_set_qdata(G_OBJECT(mi), SYS_MENU_HEAD_ITEM_ID, GINT_TO_POINTER(1));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

    if (!m->menu_reload_timeout_cb)
    {
        m->menu_reload_timeout_cb = g_timeout_add(500, (GSourceFunc) on_timeout_reload_menu, m);
    }

    plugin_set_has_system_menu(p, TRUE);
}

