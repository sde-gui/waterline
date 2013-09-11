
static gboolean recent_documents_filter (const GtkRecentFilterInfo *filter_info, gpointer user_data)
{
    if (!filter_info)
         return FALSE;

    if (!filter_info->uri)
         return FALSE;
    if (!has_case_prefix(filter_info->uri, "file:/"))
        return TRUE;

    gchar * filename = g_filename_from_uri(filter_info->uri, NULL, NULL);

    if (!filename)
        return FALSE;

    gboolean result = FALSE;

    struct stat stat_buf;
    if (stat(filename, &stat_buf) == 0)
        result = TRUE;

    g_free(filename);

    return result;
}

static void
recent_documents_activate_cb (GtkRecentChooser *chooser, Plugin * p)
{
    GtkRecentInfo * recent_info = gtk_recent_chooser_get_current_item (chooser);
    const char    * uri = gtk_recent_info_get_uri (recent_info);

    wtl_open_in_file_manager(uri);

    gtk_recent_info_unref (recent_info);
}

static void
read_recent_documents_menu(GtkMenu* menu, Plugin *p, json_t * json_item)
{
    menup *m = PRIV(p);

    int limit = su_json_dot_get_int(json_item, "limit", 20);
    gboolean show_private = su_json_dot_get_bool(json_item, "show_private", FALSE);
    gboolean local_only = su_json_dot_get_bool(json_item, "local_only", FALSE);
    gboolean show_tips = su_json_dot_get_bool(json_item, "show_tips", FALSE);

    GtkRecentManager * rm = gtk_recent_manager_get_default();

    GtkWidget      *recent_menu;
    GtkWidget      *menu_item;

    menu_item = gtk_image_menu_item_new_with_label(_("Recent Documents"));
    recent_menu = gtk_recent_chooser_menu_new_for_manager(rm);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), recent_menu);

    GdkPixbuf * pixbuf = wtl_load_icon("document-open-recent", m->iconsize, m->iconsize, FALSE);
    if (pixbuf)
    {
        GtkWidget* img = gtk_image_new_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(menu_item), img);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    gtk_widget_show_all(menu_item);

    /*
        Oh, Gtk+ has a couple of bugs here.

        First, gtk_recent_chooser_set_show_tips() has no effect
        with GtkRecentChooserMenu. So we use a custom filter to
        do the things.

        Second, get_is_recent_filtered() in gtkrecentchooserutils.c
        nulls filter_info.uri field,
        if GTK_RECENT_FILTER_DISPLAY_NAME flag not supplied.
    */
    GtkRecentFilter * filter = gtk_recent_filter_new();
    gtk_recent_filter_add_custom(filter, GTK_RECENT_FILTER_URI | GTK_RECENT_FILTER_DISPLAY_NAME/* | GTK_RECENT_FILTER_APPLICATION*/,
        recent_documents_filter, NULL, NULL);
    gtk_recent_chooser_set_filter(GTK_RECENT_CHOOSER(recent_menu), filter);

    gtk_recent_chooser_set_show_private(GTK_RECENT_CHOOSER(recent_menu), show_private);
    gtk_recent_chooser_set_local_only(GTK_RECENT_CHOOSER(recent_menu), local_only);
    gtk_recent_chooser_set_show_not_found(GTK_RECENT_CHOOSER(recent_menu), FALSE); // XXX: Seems not working.
    gtk_recent_chooser_set_show_tips(GTK_RECENT_CHOOSER(recent_menu), show_tips);
    gtk_recent_chooser_set_sort_type(GTK_RECENT_CHOOSER(recent_menu), GTK_RECENT_SORT_MRU);
    gtk_recent_chooser_set_limit(GTK_RECENT_CHOOSER(recent_menu), limit);

    g_signal_connect(G_OBJECT(recent_menu), "item-activated", G_CALLBACK(recent_documents_activate_cb), p);
}

