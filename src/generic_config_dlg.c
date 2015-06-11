/**
 *
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

#include <waterline/global.h>
#include "plugin_private.h"
#include <waterline/panel.h>
#include "panel_private.h"
#include <waterline/misc.h>
#include "bg.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <glib/gi18n.h>

static void notify_apply_config( GtkWidget* widget )
{
    GSourceFunc apply_func;
    GtkWidget* dlg;

    dlg = gtk_widget_get_toplevel( widget );
    apply_func = g_object_get_data( G_OBJECT(dlg), "apply_func" );
    if( apply_func )
        (*apply_func)( g_object_get_data(G_OBJECT(dlg), "plugin") );
}

gboolean on_entry_focus_out( GtkWidget* edit, GdkEventFocus *evt, gpointer user_data )
{
    char** val = (char**)user_data;
    const char *new_val;
    g_free( *val );
    new_val = gtk_entry_get_text((GtkEntry*)edit);
    *val = (new_val && *new_val) ? g_strdup( new_val ) : NULL;
    notify_apply_config( edit );
    return FALSE;
}

static void on_spin_changed( GtkSpinButton* spin, gpointer user_data )
{
    int* val = (int*)user_data;
    *val = (int)gtk_spin_button_get_value( spin );
    notify_apply_config( GTK_WIDGET(spin) );
}

static void on_toggle_changed( GtkToggleButton* btn, gpointer user_data )
{
    gboolean* val = (gboolean*)user_data;
    *val = gtk_toggle_button_get_active( btn );
    notify_apply_config( GTK_WIDGET(btn) );
}

static void on_enum_changed( GtkComboBox *widget, gpointer user_data )
{
    int* val = (gboolean*)user_data;
    int v = gtk_combo_box_get_active( widget );
    if (*val != v) {
        *val = gtk_combo_box_get_active( widget );
        notify_apply_config( GTK_WIDGET(widget) );
    }
}

static void on_file_chooser_btn_file_set(GtkFileChooser* btn, char** val)
{
    g_free( *val );
    *val = gtk_file_chooser_get_filename(btn);
    notify_apply_config( GTK_WIDGET(btn) );
}

static void on_color_chooser_btn_color_set(GtkColorButton* btn, char** val)
{
    g_free( *val );
    GdkColor c;
    gtk_color_button_get_color( btn, &c );
    *val = gdk_color_to_string(&c);
    notify_apply_config( GTK_WIDGET(btn) );
}

static void on_color_chooser_btn_rgba_set(GtkColorButton* button, GdkRGBA * rgba)
{
    GdkColor color; guint16  alpha;
    gtk_color_button_get_color(button, &color);
    alpha = gtk_color_button_get_alpha(button);
    color_to_rgba(rgba, &color, &alpha);
    notify_apply_config(GTK_WIDGET(button));
}


static void on_browse_btn_clicked(GtkButton* btn, GtkEntry* entry)
{
    char* file;
    GtkFileChooserAction action = (GtkFileChooserAction) g_object_get_data(G_OBJECT(btn), "chooser-action");
    GtkWidget* dlg = GTK_WIDGET(g_object_get_data(G_OBJECT(btn), "dlg"));    
    GtkWidget* fc = gtk_file_chooser_dialog_new(
                                        (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) ? _("Select a directory") : _("Select a file"),
                                        GTK_WINDOW(dlg),
                                        action,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OK, GTK_RESPONSE_OK,
                                        NULL);
    gtk_dialog_set_alternative_button_order(GTK_DIALOG(fc), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
    file = (char*)gtk_entry_get_text(entry);
    if( file && *file )
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(fc), file );
    if( gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_OK )
    {
        file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
        gtk_entry_set_text(entry, file);
        on_entry_focus_out(GTK_WIDGET(entry), NULL, g_object_get_data(G_OBJECT(dlg), "file-val"));
        g_free(file);
    }
    gtk_widget_destroy(fc);
}

static void generic_config_dlg_save(gpointer panel_gpointer,GObject *where_the_object_was)
{
    Panel *panel = (Panel *)(panel_gpointer);
    panel_save_configuration(panel);
}

/* Handler for "response" signal from standard configuration dialog. */
static void generic_config_dlg_response(GtkWidget * widget, int response, Plugin * plugin)
{
    plugin->panel->pref_dialog.plugin_pref_dialog = NULL;
    gtk_widget_destroy(widget);
}

static gboolean  completion_match_cb(GtkEntryCompletion *completion, const gchar *key, GtkTreeIter *iter, gpointer user_data)
{
    gboolean ret = FALSE;

    gchar *item = NULL;
    gchar *normalized_string;
    gchar *case_normalized_string;
  
    GtkTreeModel * completion_model = (GtkTreeModel *) user_data;

    if (completion_model)
        gtk_tree_model_get(completion_model, iter, 0, &item, -1);

    if (item != NULL)
    {
        normalized_string = g_utf8_normalize(item, -1, G_NORMALIZE_ALL);

        if (normalized_string != NULL)
        {
            case_normalized_string = g_utf8_casefold(normalized_string, -1);

            if (strstr(case_normalized_string, key))
                ret = TRUE;

            g_free (case_normalized_string);
        }
        g_free (normalized_string);
    }

    g_free (item);

    return ret;
}

/* Parameters: const char* name, gpointer ret_value, GType type, ....NULL */
GtkWidget* create_generic_config_dialog( const char* title, GtkWidget* parent,
                                      GSourceFunc apply_func, Plugin * plugin,
                                      const char* nm, ... )
{
    if (wtl_is_in_kiosk_mode())
        return NULL;

    va_list args;
    Panel* p = plugin->panel;
    GtkWidget* dlg = gtk_dialog_new_with_buttons( title, NULL, 0,
                                                  GTK_STOCK_CLOSE,
                                                  GTK_RESPONSE_CLOSE,
                                                  NULL );
    panel_apply_icon(GTK_WINDOW(dlg));

    g_signal_connect( dlg, "response", G_CALLBACK(generic_config_dlg_response), plugin);
    g_object_weak_ref(G_OBJECT(dlg), generic_config_dlg_save, p);
    if( apply_func )
        g_object_set_data( G_OBJECT(dlg), "apply_func", apply_func );
    if( plugin )
        g_object_set_data( G_OBJECT(dlg), "plugin", plugin );

    gtk_box_set_spacing( GTK_BOX(GTK_DIALOG(dlg)->vbox), 4 );

    gchar** params = NULL;

    /*
      Widgets can be added either directly to the dialog box (GTK_DIALOG(dlg)->vbox) or to the notebook widget.
      The notebook widget and its first page created on first CONF_TYPE_BEGIN_PAGE entry.
      Next CONF_TYPE_BEGIN_PAGE entries create new pages in the notebook.
    */

    GtkWidget* notebook = NULL;
    GtkWidget* frame = NULL;

    /*
      Widgets can also be arranged in table.
      CONF_TYPE_BEGIN_PAGE creates a new table. Next widgets will be placed on the table.
      CONF_TYPE_END_TABLE resers the table pointer, so next widgets will be placed directly on the dialog box (or notebook page).
    */

    GtkWidget* table = NULL;
    int table_row_count = 0;

    gboolean only_entry = FALSE;
    gboolean create_browse_button = FALSE;
    gboolean ignore_frame = FALSE;

    /* Previous widget data. Used to deal with widget properties. */

    GType prev_type = -1;
    GtkWidget* prev_entry = NULL;
    GtkWidget* prev_label = NULL;

    /*  */

    const char* name = nm;
    va_start( args, nm );
    while( name )
    {
        /* Read next entry data */

        gpointer val = va_arg( args, gpointer );
        GType type = va_arg( args, GType );

        if (type == CONF_TYPE_ENUM) {
            char d[2] = {name[0], 0};
            params = g_strsplit(name, d, 9999);
            name = params[1];
        }

        /* These variables are filled in entry switch statement: */

        GtkWidget* entry = NULL; /* Widget to add. */
        only_entry = FALSE;      /* Do not add label. */
        create_browse_button = FALSE; /* Create browse button for the widget. */
        ignore_frame = FALSE;    /* Ignore notebook page, i.e. act as if frame == NULL. Used to create notebook itself. */
        gboolean is_property = FALSE; /* The entry is property for previous widget. */


        switch( type )
        {
            case CONF_TYPE_SET_PROPERTY:
            {
                 is_property = TRUE;
                 if (!prev_entry)
                     break;
                 if (!val)
                     break;
                 if (strcmp(name, "tooltip-text") == 0) {
                     gtk_widget_set_tooltip_text(prev_entry, (const gchar *) val);
                     if (prev_label)
                         gtk_widget_set_tooltip_text(prev_label, (const gchar *) val);
                 } else if (strcmp(name, "tooltip-markup") == 0) {
                     gtk_widget_set_tooltip_markup(prev_entry, (const gchar *) val);
                     if (prev_label)
                         gtk_widget_set_tooltip_markup(prev_label, (const gchar *) val);
                 } else if (strcmp(name, "int-min-value") == 0 || strcmp(name, "int-max-value") == 0) {
                     if (prev_type == CONF_TYPE_INT)
                     {
                         gdouble min_v, max_v;
                         gtk_spin_button_get_range(GTK_SPIN_BUTTON(prev_entry), &min_v, &max_v);
                         if (strcmp(name, "int-min-value") == 0)
                             min_v = *(int*)val;
                         else
                             max_v = *(int*)val;
                         gtk_spin_button_set_range(GTK_SPIN_BUTTON(prev_entry), min_v, max_v);
                     }
                 } else if (strcmp(name, "completion-list") == 0) {
                     if (prev_type == CONF_TYPE_STR)
                     {
                         GtkEntryCompletion *completion;
                         GtkTreeModel *completion_model;

                         /* Create the completion object */
                         completion = gtk_entry_completion_new ();

                         /* Assign the completion to the entry */
                         gtk_entry_set_completion (GTK_ENTRY (prev_entry), completion);
                         g_object_unref (completion);

                         /* Create a tree model and use it as the completion model */
                         GtkListStore *store;
                         GtkTreeIter iter;
                         int i;
                         store = gtk_list_store_new (1, G_TYPE_STRING);

                         char d[2] = {((char *)val)[0], 0};
                         char ** strings = g_strsplit((char *)val, d, 9999);

                         for (i = 0; strings[i]; i++)
                         {
                             gtk_list_store_append (store, &iter);
                             gtk_list_store_set (store, &iter, 0, strings[i], -1);
                         }

                         g_strfreev(strings);

                         completion_model = GTK_TREE_MODEL (store);
                         gtk_entry_completion_set_model (completion, completion_model);
                         g_object_unref (completion_model);

                         gtk_entry_completion_set_match_func(completion, completion_match_cb, completion_model, NULL);

                         /* Use model column 0 as the text column */
                         gtk_entry_completion_set_text_column (completion, 0);
                     }
                 }
                 break;
            }
            case CONF_TYPE_BEGIN_PAGE:
            {
                if (!notebook) {
                    notebook = gtk_notebook_new();
                    entry = notebook;
                }
                frame = gtk_vbox_new( FALSE, 4 );
                GtkWidget* b = gtk_hbox_new( FALSE, 4 );
                gtk_box_pack_start( GTK_BOX(b), frame, FALSE, FALSE, 4 );

                GtkWidget* label = gtk_label_new(name);
                gtk_notebook_append_page( GTK_NOTEBOOK(notebook), b, label );
                table = NULL;
                only_entry = TRUE;
                ignore_frame = TRUE;
                break;
            }
            case CONF_TYPE_END_PAGE:
                table = NULL;
                frame = NULL;
                break;
            case CONF_TYPE_BEGIN_TABLE:
                entry = table = gtk_table_new(1, 3, FALSE);
                gtk_table_set_col_spacings(GTK_TABLE(table), 4);
                gtk_table_set_row_spacings(GTK_TABLE(table), 8);
                table_row_count = 0;
                only_entry = TRUE;
                break;
            case CONF_TYPE_END_TABLE:
                table = NULL;
                only_entry = TRUE;
                break;
            case CONF_TYPE_STR:
            case CONF_TYPE_FILE_ENTRY: /* entry with a button to browse for files. */
            case CONF_TYPE_DIRECTORY_ENTRY: /* entry with a button to browse for directories. */
                entry = gtk_entry_new();
                if( *(char**)val )
                    gtk_entry_set_text( GTK_ENTRY(entry), *(char**)val );
                gtk_entry_set_width_chars(GTK_ENTRY(entry), 40);
                g_signal_connect( entry, "focus-out-event",
                  G_CALLBACK(on_entry_focus_out), val );
                create_browse_button = (type == CONF_TYPE_FILE_ENTRY) || (type == CONF_TYPE_DIRECTORY_ENTRY);
                break;
            case CONF_TYPE_INT:
            {
                int v = *(int*)val;
                int min_v = (v < 0) ? v : 0;
                int max_v = (v > 1000) ? v : 1000;
                entry = gtk_spin_button_new_with_range( min_v, max_v, 1 );
                gtk_spin_button_set_value( GTK_SPIN_BUTTON(entry), v );
                g_signal_connect( entry, "value-changed",
                  G_CALLBACK(on_spin_changed), val );
                break;
            }
            case CONF_TYPE_BOOL:
                entry = gtk_check_button_new();
                GtkWidget* label = gtk_label_new( name );
                gtk_container_add( GTK_CONTAINER(entry), label );
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(entry), *(gboolean*)val );
                g_signal_connect( entry, "toggled",
                  G_CALLBACK(on_toggle_changed), val );
                only_entry = TRUE;
                break;
            case CONF_TYPE_FILE:
                entry = gtk_file_chooser_button_new(_("Select a file"), GTK_FILE_CHOOSER_ACTION_OPEN);
                if( *(char**)val )
                    gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(entry), *(char**)val );
                g_signal_connect( entry, "file-set",
                  G_CALLBACK(on_file_chooser_btn_file_set), val );
                break;
            case CONF_TYPE_COLOR:
                entry = gtk_color_button_new();
                if( *(char**)val )
                {
                    GdkColor c;
                    gdk_color_parse(*(char**)val,  &c);
                    gtk_color_button_set_color (GTK_COLOR_BUTTON(entry), &c);
                }
                g_signal_connect( entry, "color-set",
                  G_CALLBACK(on_color_chooser_btn_color_set), val );
                break;
            case CONF_TYPE_RGBA:
            {
                entry = gtk_color_button_new();
                GdkRGBA * rgba = (GdkRGBA *) val;
                GdkColor color; guint16 alpha;
                rgba_to_color(rgba, &color, &alpha);
                gtk_color_button_set_color(GTK_COLOR_BUTTON(entry), &color);
                gtk_color_button_set_alpha(GTK_COLOR_BUTTON(entry), alpha);
                gtk_color_button_set_use_alpha(GTK_COLOR_BUTTON(entry), TRUE);
                g_signal_connect(entry, "color-set", G_CALLBACK(on_color_chooser_btn_rgba_set), rgba);
                break;
            }
            case CONF_TYPE_TRIM:
            case CONF_TYPE_TITLE:
            {
                entry = gtk_label_new(NULL);
                const char* style = (type == CONF_TYPE_TITLE) ? "<span weight=\"bold\">%s</span>" : "<span style=\"italic\">%s</span>";
                char *markup = g_markup_printf_escaped (style, name );
                gtk_label_set_markup (GTK_LABEL (entry), markup);
                g_free (markup);
                only_entry = TRUE;
                break;
            }
            case CONF_TYPE_ENUM:
                entry = gtk_combo_box_new_text();
                int i;
                for (i = 2; params[i]; i++) {
                    gtk_combo_box_append_text(GTK_COMBO_BOX(entry), params[i]);
                }
                gtk_combo_box_set_active(GTK_COMBO_BOX(entry), *(int*)val);
                //gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(entry), *(gboolean*)val );
                g_signal_connect( entry, "changed",
                  G_CALLBACK(on_enum_changed), val );
                break;
            default:
                g_printerr("Invalid CONF_TYPE: %d (text: %s)\n", type, name);
                break;
        }

        GtkWidget* label = NULL;

        if( entry )
        {
            GtkBox * vbox = (frame && !ignore_frame) ? GTK_BOX(frame) : GTK_BOX(GTK_DIALOG(dlg)->vbox);

            if (only_entry)
            {
                int r = table_row_count;
                if (type == CONF_TYPE_BEGIN_TABLE || !table)
                    gtk_box_pack_start( GTK_BOX(vbox), entry, FALSE, FALSE, 2 );
                else
                    gtk_table_resize(GTK_TABLE(table), ++table_row_count, 3),
                    gtk_table_attach_defaults(GTK_TABLE(table), entry, 0, 3, r, r + 1);
            }
            else
            {
                label = gtk_label_new( name );
                GtkWidget* browse = NULL;
                if (create_browse_button) {
                    browse = gtk_button_new_with_mnemonic(_("_Browse"));
                    g_object_set_data(G_OBJECT(dlg), "file-val", val);
                    g_object_set_data(G_OBJECT(browse), "dlg", dlg);
                    g_object_set_data(G_OBJECT(browse), "chooser-action",
                        (gpointer) ((type == CONF_TYPE_DIRECTORY_ENTRY) ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER : GTK_FILE_CHOOSER_ACTION_OPEN));
                    g_signal_connect( browse, "clicked", G_CALLBACK(on_browse_btn_clicked), entry );
                }

                if (table) {
                    int r = table_row_count;                    
                    gtk_table_resize(GTK_TABLE(table), ++table_row_count, 3),
                    gtk_table_attach(GTK_TABLE(table), label, 0, 1, r, r + 1, GTK_FILL, GTK_FILL, 0, 0);
                    if (browse) {
                        gtk_table_attach_defaults(GTK_TABLE(table), entry, 1, 2, r, r + 1);
                        gtk_table_attach(GTK_TABLE(table), browse, 2, 3, r, r + 1, GTK_SHRINK, GTK_FILL, 0, 0);
                    } else {
                        gtk_table_attach_defaults(GTK_TABLE(table), entry, 1, 3, r, r + 1);
                    }
                } else {
                    GtkWidget* hbox = gtk_hbox_new( FALSE, 2 );
                    gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 2 );
                    gtk_box_pack_start( GTK_BOX(hbox), entry, TRUE, TRUE, 2 );
                    if (browse)
                    {
                        gtk_box_pack_start( GTK_BOX(hbox), browse, TRUE, TRUE, 2 );
                    }
                    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, FALSE, 2 );
                }
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
            }
        }
        if (params != NULL) {
            g_strfreev(params);
            params = NULL;
        }

        if (!is_property) {
            prev_type  = type;
            prev_entry = entry;
            prev_label = label;
        }

        name = va_arg( args, const char* );
    }
    va_end( args );

    gtk_container_set_border_width( GTK_CONTAINER(dlg), 8 );

    /* If there is already a plugin configuration dialog open, close it.
     * Then record this one in case the panel or plugin is deleted. */
    if (p->pref_dialog.plugin_pref_dialog != NULL)
        gtk_widget_destroy(p->pref_dialog.plugin_pref_dialog);
    p->pref_dialog.plugin_pref_dialog = dlg;

    gtk_widget_show_all( dlg );
    return dlg;
}
