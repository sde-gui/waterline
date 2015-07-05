/**
 *
 * Copyright (c) 2011-2013 Vadim Ushakov
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
#include "plugin_internal.h"
#include "plugin_private.h"
#include <waterline/panel.h>
#include "panel_private.h"
#include <waterline/paths.h>
#include <waterline/misc.h>
#include <waterline/defaultapplications.h>
#include "bg.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <glib/gi18n.h>

enum{
    COL_NAME,
    COL_EXPAND,
    COL_DATA,
    N_COLS
};

static void modify_plugin( GtkTreeView* view );

static gchar * menu_item_plugin_type_key = NULL;

/******************************************************************************/

static void
on_sel_plugin_changed( GtkTreeSelection* tree_sel, GtkWidget* label )
{
    GtkTreeIter it;
    GtkTreeModel* model;
    Plugin* pl;

    if( gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
    {
        GtkTreeView* view = gtk_tree_selection_get_tree_view( tree_sel );
        GtkWidget *edit_btn = GTK_WIDGET(g_object_get_data( G_OBJECT(view), "edit_btn" ));
        gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );
        gtk_label_set_text( GTK_LABEL(label), _(pl->class->description) );
        gtk_widget_set_sensitive(edit_btn, pl->class->show_properties != NULL);
    }
}

static void
on_plugin_expand_toggled(GtkCellRendererToggle* render, char* path, GtkTreeView* view)
{
    GtkTreeModel* model;
    GtkTreeIter it;
    GtkTreePath* tp = gtk_tree_path_new_from_string( path );
    model = gtk_tree_view_get_model( view );
    if( gtk_tree_model_get_iter( model, &it, tp ) )
    {
        Plugin* pl;
        gboolean old_expand, expand, fill;
        guint padding;
        GtkPackType pack_type;

        gtk_tree_model_get( model, &it, COL_DATA, &pl, COL_EXPAND, &expand, -1 );

        if (pl->class->expand_available)
        {
            /* Only honor "stretch" if allowed by the plugin. */
            expand = ! expand;
            pl->expand = expand;
            gtk_list_store_set( (GtkListStore*)model, &it, COL_EXPAND, expand, -1 );

            /* Query the old packing of the plugin widget.
             * Apply the new packing with only "expand" modified. */
            gtk_box_query_child_packing( GTK_BOX(pl->panel->plugin_box), pl->pwid, &old_expand, &fill, &padding, &pack_type );
            gtk_box_set_child_packing( GTK_BOX(pl->panel->plugin_box), pl->pwid, expand, fill, padding, pack_type );
            panel_preferences_changed(pl->panel, 0);
        }
    }
    gtk_tree_path_free( tp );
}

static void on_stretch_render(GtkTreeViewColumn * column, GtkCellRenderer * renderer, GtkTreeModel * model, GtkTreeIter * iter, gpointer data)
{
    /* Set the control visible depending on whether stretch is available for the plugin.
     * The g_object_set method is touchy about its parameter, so we can't pass the boolean directly. */
    Plugin * pl;
    gtk_tree_model_get(model, iter, COL_DATA, &pl, -1);
    g_object_set(renderer,
        "visible", ((pl->class->expand_available) ? TRUE : FALSE),
        NULL);
}

static void init_plugin_list( Panel* p, GtkTreeView* view, GtkWidget* label )
{
    GtkListStore* list;
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;
    GtkTreeSelection* tree_sel;
    GList* l;
    GtkTreeIter it;

    g_object_set_data( G_OBJECT(view), "panel", p );

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes(
            _("Currently loaded plugins"),
            render, "text", COL_NAME, NULL );
    gtk_tree_view_column_set_expand( col, TRUE );
    gtk_tree_view_append_column( view, col );

    render = gtk_cell_renderer_toggle_new();
    g_object_set( render, "activatable", TRUE, NULL );
    g_signal_connect( render, "toggled", G_CALLBACK( on_plugin_expand_toggled ), view );
    col = gtk_tree_view_column_new_with_attributes(
            _("Stretch"),
            render, "active", COL_EXPAND, NULL );
    gtk_tree_view_column_set_expand( col, FALSE );
    gtk_tree_view_column_set_cell_data_func(col, render, on_stretch_render, NULL, NULL);
    gtk_tree_view_append_column( view, col );

    list = gtk_list_store_new( N_COLS, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER );
    for( l = p->plugins; l; l = l->next )
    {
        GtkTreeIter it;
        Plugin* pl = (Plugin*)l->data;
        gtk_list_store_append( list, &it );
        gtk_list_store_set( list, &it,
                            COL_NAME, _(pl->class->name),
                            COL_EXPAND, pl->expand,
                            COL_DATA, pl,
                            -1);
    }
    gtk_tree_view_set_model( view, GTK_TREE_MODEL( list ) );
    g_signal_connect( view, "row-activated",
                      G_CALLBACK(modify_plugin), NULL );
    tree_sel = gtk_tree_view_get_selection( view );
    gtk_tree_selection_set_mode( tree_sel, GTK_SELECTION_BROWSE );
    g_signal_connect( tree_sel, "changed",
                      G_CALLBACK(on_sel_plugin_changed), label);
    if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL(list), &it ) )
        gtk_tree_selection_select_iter( tree_sel, &it );
}

static void on_add_plugin_menu_item_activate(GtkWidget * menu_item, GtkTreeView * _view)
{
    Panel * panel = (Panel *) g_object_get_data( G_OBJECT(_view), "panel" );
    const gchar * type = (const gchar *) g_object_get_data((GObject *) menu_item, menu_item_plugin_type_key);

    Plugin * pl = plugin_load(type);
    if (pl)
    {
        pl->panel = panel;
        if (pl->class->expand_default)
            pl->expand = TRUE;
        plugin_start(pl);
        panel->plugins = g_list_append(panel->plugins, pl);
        panel_save_configuration(panel);

        if (pl->pwid)
        {
            gtk_widget_show(pl->pwid);
        }

        {
            GtkTreePath* tree_path;
            GtkTreeSelection* tree_sel;
            GtkTreeIter it;
            GtkTreeModel* model;

            model = gtk_tree_view_get_model( _view );
            gtk_list_store_append( (GtkListStore*)model, &it );
            gtk_list_store_set( (GtkListStore*)model, &it,
                COL_NAME, _(pl->class->name),
                COL_EXPAND, pl->expand,
                COL_DATA, pl, -1 );
            tree_sel = gtk_tree_view_get_selection( _view );
            gtk_tree_selection_select_iter( tree_sel, &it );
            if ((tree_path = gtk_tree_model_get_path(model, &it)) != NULL)
            {
                gtk_tree_view_scroll_to_cell( _view, tree_path, NULL, FALSE, 0, 0 );
                gtk_tree_path_free( tree_path );
            }
        }
    }

}

static void on_add_plugin(GtkButton * _button, GtkTreeView * _view)
{
    GtkWidget * menu = gtk_menu_new();
    GtkWidget ** submenus = g_new0(GtkWidget *, NR_PLUGIN_CATEGORY);
    GtkWidget ** submenus_mi = g_new0(GtkWidget *, NR_PLUGIN_CATEGORY);

    {
        PLUGIN_CATEGORY i;
        for (i = 0; i < NR_PLUGIN_CATEGORY; i++)
        {
            PLUGIN_CATEGORY c = (i == NR_PLUGIN_CATEGORY - 1) ? 0 : i + 1;
            submenus[c] = gtk_menu_new();
            const char * label = "";
            switch (c)
            {
                case PLUGIN_CATEGORY_UNKNOWN:      label = _("Miscellaneous"); break;
                case PLUGIN_CATEGORY_WINDOW_MANAGEMENT: label = _("Window Management"); break;
                case PLUGIN_CATEGORY_LAUNCHER:     label = _("Launchers"); break;
                case PLUGIN_CATEGORY_SW_INDICATOR: label = _("Notifications and Indicators"); break;
                case PLUGIN_CATEGORY_HW_INDICATOR: label = _("Hardware Monitoring and Control"); break;
#ifdef PRODUCE_SWITCH_WARNING
                case NR_PLUGIN_CATEGORY:
#else
                default:
#endif
                    label = _("Miscellaneous"); break;
            }
            submenus_mi[c] = gtk_menu_item_new_with_label(label);
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenus_mi[c]), submenus[c]);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), submenus_mi[c]);
        }
    }

    {
        if (!menu_item_plugin_type_key) {
            menu_item_plugin_type_key = g_strdup_printf("waterline menu_item_plugin_type_key %x%x",
                (unsigned) g_random_int(), (unsigned) g_random_int());
        }
    }


    {
        GList * classes = plugin_get_available_classes();
        GList * iter;
        for (iter = classes; iter; iter = iter->next) {
            PluginClass * pc = (PluginClass *) iter->data;
            PLUGIN_CATEGORY category = pc->category;
            if (category >= NR_PLUGIN_CATEGORY || category < 0)
                category = 0;
            GtkWidget * menu_item = gtk_menu_item_new_with_label(_(pc->name));
            if (pc->description)
                gtk_widget_set_tooltip_text(menu_item, _(pc->description));
            gtk_menu_shell_append(GTK_MENU_SHELL(submenus[category]), menu_item);
            gtk_widget_show(menu_item);
            gtk_widget_show(submenus_mi[category]);
            gtk_widget_set_sensitive(menu_item, (!pc->one_per_system) || (!pc->one_per_system_instantiated));
            g_object_set_data_full(G_OBJECT(menu_item), menu_item_plugin_type_key, g_strdup(pc->type), g_free);

            g_signal_connect(G_OBJECT(menu_item), "activate", (GCallback) on_add_plugin_menu_item_activate, _view);
        }
        plugin_class_list_free(classes);
    }

    g_free(submenus);
    g_free(submenus_mi);

    gtk_menu_popup(GTK_MENU(menu),
                   NULL, NULL,
                   NULL, NULL,
                   0, 0);
}

void configurator_remove_plugin_from_list(Panel * p, Plugin * pl);

void configurator_remove_plugin_from_list(Panel * p, Plugin * pl)
{
     GtkTreeView * view = NULL;
     if (p && p->pref_dialog.pref_dialog)
         view = (GtkTreeView *) g_object_get_data( G_OBJECT(p->pref_dialog.pref_dialog), "plugin_list");

     if (!view)
         return;

     GtkTreeModel* model = gtk_tree_view_get_model(view);
     if (!model)
         return;

     GtkTreeIter it;
     if (gtk_tree_model_get_iter_first(model, &it))
     {
         do
         {
             Plugin* pl1;
             gtk_tree_model_get( model, &it, COL_DATA, &pl1, -1 );
             if (pl1 == pl)
             {
                 gtk_list_store_remove( GTK_LIST_STORE(model), &it );
                 break;
             }
         } while (gtk_tree_model_iter_next(model, &it));
     }
}

static void on_remove_plugin( GtkButton* btn, GtkTreeView* view )
{
    GtkTreeIter it;
    GtkTreePath* tree_path;
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection( view );
    Plugin* pl;

    Panel* p = (Panel*) g_object_get_data( G_OBJECT(view), "panel" );

    if( gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
    {
        tree_path = gtk_tree_model_get_path( model, &it );
        gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );
        if( gtk_tree_path_get_indices(tree_path)[0] >= gtk_tree_model_iter_n_children( model, NULL ) )
            gtk_tree_path_prev( tree_path );
        gtk_list_store_remove( GTK_LIST_STORE(model), &it );
        gtk_tree_selection_select_path( tree_sel, tree_path );
        gtk_tree_path_free( tree_path );

        p->plugins = g_list_remove( p->plugins, pl );
        plugin_delete(pl);
        panel_save_configuration(p);
    }
}

void modify_plugin( GtkTreeView* view )
{
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection( view );
    GtkTreeModel* model;
    GtkTreeIter it;
    Plugin* pl;

    if( ! gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
        return;

    gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );
    if (pl->class->show_properties)
        pl->class->show_properties(pl, (GtkWindow*)gtk_widget_get_toplevel(GTK_WIDGET(view)));
}

static int get_widget_index(    Panel* p, Plugin* pl )
{
    GList* l;
    int i;
    for( i = 0, l = p->plugins; l; l = l->next )
    {
        Plugin* _pl = (Plugin*)l->data;
        if( _pl == pl )
            return i;
        if( _pl->pwid )
            ++i;
    }
    return -1;
}

static void on_moveup_plugin(  GtkButton* btn, GtkTreeView* view )
{
    GList *l;
    GtkTreeIter it, prev;
    GtkTreeModel* model = gtk_tree_view_get_model( view );
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection( view );
    int i;

    Panel* panel = (Panel*) g_object_get_data( G_OBJECT(view), "panel" );

    if( ! gtk_tree_model_get_iter_first( model, &it ) )
        return;
    if( gtk_tree_selection_iter_is_selected( tree_sel, &it ) )
        return;
    do{
        if( gtk_tree_selection_iter_is_selected(tree_sel, &it) )
        {
            Plugin* pl;
            gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );
            gtk_list_store_move_before( GTK_LIST_STORE( model ),
                                        &it, &prev );

            i = 0;
            for( l = panel->plugins; l; l = l->next, ++i )
            {
                if( l->data == pl  )
                {
                    panel->plugins = g_list_insert( panel->plugins, pl, i - 1);
                    panel->plugins = g_list_delete_link( panel->plugins, l);
                }
            }
            if( pl->pwid )
            {
                gtk_box_reorder_child( GTK_BOX(panel->plugin_box), pl->pwid, get_widget_index( panel, pl ) );
            }
            panel_save_configuration(panel);
            return;
        }
        prev = it;
    }while( gtk_tree_model_iter_next( model, &it ) );
}

static void on_movedown_plugin(  GtkButton* btn, GtkTreeView* view )
{
    GList *l;
    GtkTreeIter it, next;
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection( view );
    Plugin* pl;
    int i;

    Panel* panel = (Panel*) g_object_get_data( G_OBJECT(view), "panel" );

    if( ! gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
        return;
    next = it;

    if( ! gtk_tree_model_iter_next( model, &next) )
        return;

    gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );

    gtk_list_store_move_after( GTK_LIST_STORE( model ), &it, &next );

    i = 0;
    for( l = panel->plugins; l; l = l->next, ++i )
    {
        if( l->data == pl  )
        {
            panel->plugins = g_list_insert( panel->plugins, pl, i + 2);
            panel->plugins = g_list_delete_link( panel->plugins, l);
        }
    }
    if( pl->pwid )
    {
        gtk_box_reorder_child( GTK_BOX(panel->plugin_box), pl->pwid, get_widget_index( panel, pl ) );
    }
    panel_save_configuration(panel);
}

void initialize_plugin_list(Panel * p, GtkBuilder * builder)
{
    GtkWidget * plugin_list = (GtkWidget*)gtk_builder_get_object( builder, "plugin_list" );
    GtkWidget * w;

    g_object_set_data( G_OBJECT(p->pref_dialog.pref_dialog), "plugin_list", plugin_list );

    w = (GtkWidget*)gtk_builder_get_object( builder, "add_btn" );
    g_signal_connect( w, "clicked", G_CALLBACK(on_add_plugin), plugin_list );

    w = (GtkWidget*)gtk_builder_get_object( builder, "edit_btn" );
    g_signal_connect_swapped( w, "clicked", G_CALLBACK(modify_plugin), plugin_list );
    g_object_set_data( G_OBJECT(plugin_list), "edit_btn", w );

    w = (GtkWidget*)gtk_builder_get_object( builder, "remove_btn" );
    g_signal_connect( w, "clicked", G_CALLBACK(on_remove_plugin), plugin_list );
    w = (GtkWidget*)gtk_builder_get_object( builder, "moveup_btn" );
    g_signal_connect( w, "clicked", G_CALLBACK(on_moveup_plugin), plugin_list );
    w = (GtkWidget*)gtk_builder_get_object( builder, "movedown_btn" );
    g_signal_connect( w, "clicked", G_CALLBACK(on_movedown_plugin), plugin_list );

    w = (GtkWidget*)gtk_builder_get_object( builder, "plugin_desc" );
    init_plugin_list( p, (GtkTreeView*)plugin_list, w );
}

