/**
 * Copyright (c) 2009 LxDE Developers, see the file AUTHORS for details.
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

#include <gtk/gtk.h>
#include <gtk/gtkprivate.h>
#include <string.h>

#include "icon-grid.h"
#include "panel.h"
#include "plugin.h"

//#define DEBUG

#include "dbg.h"

static gboolean icon_grid_placement(IconGrid * ig);
static void icon_grid_geometry(IconGrid * ig, gboolean layout);
static void icon_grid_element_size_request(GtkWidget * widget, GtkRequisition * requisition, IconGridElement * ige);
static void icon_grid_size_request(GtkWidget * widget, GtkRequisition * requisition, IconGrid * ig);
static void icon_grid_size_allocate(GtkWidget * widget, GtkAllocation * allocation, IconGrid * ig);
static void icon_grid_demand_resize(IconGrid * ig);

enum {
   GEOMETRY_SIZE_REQUEST,
   GEOMETRY_SIZE_ALLOCATED,
   GEOMETRY_DEMAND_RESIZE
};

/* Establish the widget placement of an icon grid. */
static gboolean icon_grid_placement(IconGrid * ig)
{
    ENTER;
    
    ig->placement_idle_cb = 0;

    //g_print("[0x%x] icon_grid_placement\n", (int)ig);

    /* Make sure the container is visible. */
    gtk_widget_show(ig->container);

    /* Erase the window. */
    GdkWindow * window = ig->widget->window;
    if (window != NULL)
        panel_determine_background_pixmap(ig->panel, ig->widget, window);

    /* Get and save the desired container geometry. */
    ig->container_width = ig->container->allocation.width;
    ig->container_height = ig->container->allocation.height;
    int child_width = ig->child_width;
    int child_height = ig->child_height;

    /* Get the required container geometry if all elements get the client's desired allocation. */
    int container_width_needed = (ig->columns * (child_width + ig->spacing)) - ig->spacing;
    int container_height_needed = (ig->rows * (child_height + ig->spacing)) - ig->spacing;

    int centering_offset_x = 0;
    int centering_offset_y = 0;

    /* Get the constrained child geometry if the allocated geometry is insufficient.
     * All children are still the same size and share equally in the deficit. */
    if ((ig->columns != 0) && (ig->rows != 0) && (ig->container_width > 1))
    {
        int width_delta  = ig->container_width  - container_width_needed;
        int height_delta = ig->container_height - container_height_needed;

        if (width_delta < 0) {
            child_width = (ig->container_width - ((ig->columns - 1) * ig->spacing)) / ig->columns;
        } else {
            if (ig->expand) {
                int extra_child_width = width_delta / ig->columns;
                width_delta = width_delta % ig->columns;
                child_width += extra_child_width;
            }
            if (ig->orientation != GTK_ORIENTATION_HORIZONTAL)
                centering_offset_x = width_delta / 2;
        }

        if (height_delta < 0) {
            child_height = (ig->container_height - ((ig->rows - 1) * ig->spacing)) / ig->rows;
        } else {
            if (ig->expand) {
                int extra_child_height = height_delta / ig->rows;
                height_delta = height_delta % ig->rows;
                child_height += extra_child_height;
            }
            if (ig->orientation == GTK_ORIENTATION_HORIZONTAL)
                centering_offset_y = height_delta / 2;
        }
    }

    ig->allocated_child_width = child_width;
    ig->allocated_child_height = child_height;

    centering_offset_x = centering_offset_x < ig->border ? ig->border : centering_offset_x;
    centering_offset_y = centering_offset_y < ig->border ? ig->border : centering_offset_y;

    /* Initialize parameters to control repositioning each visible child. */
    GtkTextDirection direction = gtk_widget_get_direction(ig->container);
    int limit = ((ig->orientation == GTK_ORIENTATION_HORIZONTAL)
        ? centering_offset_y + (ig->rows * (child_height + ig->spacing))
        : centering_offset_x + (ig->columns * (child_width + ig->spacing)));
    int x_initial = ((direction == GTK_TEXT_DIR_RTL)
        ? ig->widget->allocation.width - child_width - centering_offset_x
        : ig->border + centering_offset_x);
    int x_delta = child_width + ig->spacing;
    if (direction == GTK_TEXT_DIR_RTL) x_delta = - x_delta;


    IconGridElement * ige;
    for (ige = ig->child_list; ige != NULL; ige = ige->flink)
    {
        if (ige->deferred_hide && !ige->visible) {
            //g_print("[0x%x] deferred hiding 0x%x\n", (int)ig, (int)ige);
            gtk_widget_hide(ige->widget);
        }
        ige->deferred_hide = 0;
    }

    /* Reposition each visible child. */
    int x = x_initial;
    int y = centering_offset_y;
    gboolean contains_sockets = FALSE;
    for (ige = ig->child_list; ige != NULL; ige = ige->flink)
    {
        if (ige->visible)
        {
            /* Do necessary operations on the child. */
            gtk_widget_show(ige->widget);
            if (((child_width != ige->widget->allocation.width) || (child_height != ige->widget->allocation.height))
            && (child_width > 0) && (child_height > 0))
                {
                GtkAllocation alloc;
                alloc.x = x;
                alloc.y = y;
                alloc.width = child_width;
                alloc.height = child_height;
                gtk_widget_size_allocate(ige->widget, &alloc);
                gtk_widget_queue_resize(ige->widget);		/* Get labels to redraw ellipsized */
                }
            gtk_fixed_move(GTK_FIXED(ig->widget), ige->widget, x, y);
            gtk_widget_queue_draw(ige->widget);

            /* Note if a socket is placed. */
            if (GTK_IS_SOCKET(ige->widget))
                contains_sockets = TRUE;

            /* Advance to the next grid position. */
            if (ig->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
                y += child_height + ig->spacing;
                if (y >= limit)
                {
                    y = centering_offset_y;
                    x += x_delta;
                }
            }
            else
            {
                x += x_delta;
                if ((direction == GTK_TEXT_DIR_RTL) ? (x <= 0) : (x >= limit))
                {
                    x = x_initial;
                    y += child_height + ig->spacing;
                }
            }
        }
    }

    /* Redraw the container. */
//    if (window != NULL)
//        gdk_window_invalidate_rect(window, NULL, TRUE);
//    gtk_widget_queue_draw(ig->container);

    /* If the icon grid contains sockets, do special handling to get the background erased. */
    if (contains_sockets)
        plugin_widget_set_background(ig->widget, ig->panel);

    RET(FALSE);
}

/* Establish the geometry of an icon grid. */
static void icon_grid_geometry(IconGrid * ig, int reason)
{
    //g_print("[0x%x] icon_grid_geometry: reason = %d\n", (int)ig, reason);

    ENTER;

    /* Count visible children. */
    int visible_children = 0;
    IconGridElement * ige;
    for (ige = ig->child_list; ige != NULL; ige = ige->flink)
        if (ige->visible)
            visible_children += 1;

   int original_rows = ig->rows;
   int original_columns = ig->columns;

   int original_requisition_width = ig->requisition_width;
   int original_requisition_height = ig->requisition_height;

   int target_dimension = ig->target_dimension;
   if (ig->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        /* In horizontal orientation, fit as many rows into the available height as possible.
         * Then allocate as many columns as necessary.  Guard against zerodivides. */
        if (ig->container->allocation.height > 1)
            target_dimension = ig->container->allocation.height;
        ig->rows = 0;
        if ((ig->child_height + ig->spacing) != 0)
            ig->rows = (target_dimension + ig->spacing - ig->border * 2) / (ig->child_height + ig->spacing);
        if (ig->rows == 0)
            ig->rows = 1;
        ig->columns = (visible_children + (ig->rows - 1)) / ig->rows;
        if ((ig->columns == 1) && (ig->rows > visible_children))
            ig->rows = visible_children;
    }
    else
    {
        /* In vertical orientation, fit as many columns into the available width as possible.
         * Then allocate as many rows as necessary.  Guard against zerodivides. */
        if (ig->container->allocation.width > 1)
            target_dimension = ig->container->allocation.width;
        ig->columns = 0;
        if ((ig->child_width + ig->spacing) != 0)
            ig->columns = (target_dimension + ig->spacing - ig->border * 2) / (ig->child_width + ig->spacing);
        if (ig->columns == 0)
            ig->columns = 1;
        ig->rows = (visible_children + (ig->columns - 1)) / ig->columns;
        if ((ig->rows == 1) && (ig->columns > visible_children))
            ig->columns = visible_children;
    }

    /* Compute the requisition. */
    if ((ig->columns == 0) || (ig->rows == 0))
    {
        ig->requisition_width = 1;
        ig->requisition_height = 1;
    }
    else
    {
        int column_spaces = ig->columns - 1;
        int row_spaces = ig->rows - 1;
        if (column_spaces < 0) column_spaces = 0;
        if (row_spaces < 0) row_spaces = 0;
        ig->requisition_width = ig->child_width * ig->columns + column_spaces * ig->spacing + 2 * ig->border;
        ig->requisition_height = ig->child_height * ig->rows + row_spaces * ig->spacing + 2 * ig->border;
    }

    if (reason != GEOMETRY_SIZE_ALLOCATED)
        ig->requisition_changed |= (original_requisition_width != ig->requisition_width || original_requisition_height != ig->requisition_height);

    /* If the table geometry or child composition changed, redo the placement of children in table cells.
     * This is gated by having a valid table allocation and by the "layout" parameter, which prevents a recursive loop.
     * We do the placement later, also to prevent a recursive loop. */
    if ((reason != GEOMETRY_SIZE_REQUEST)
    && (( ! ig->actual_dimension)
      || (ig->requisition_changed)
      || (ig->rows != original_rows)
      || (ig->columns != original_columns)
      || (ig->container_width != ig->container->allocation.width)
      || (ig->container_height != ig->container->allocation.height)
      || (ig->children_changed)))
    {
        if (ig->requisition_changed && reason == GEOMETRY_DEMAND_RESIZE) {
            //g_print("[0x%x] gtk_widget_queue_resize()\n", (int)ig);
            gtk_widget_queue_resize(ig->container);
        } else{ 
            ig->actual_dimension = TRUE;
            ig->children_changed = FALSE;
            //g_print("[0x%x] g_idle_add((GSourceFunc) icon_grid_placement, ig)\n", (int)ig);
            if (ig->placement_idle_cb == 0)
                ig->placement_idle_cb = g_idle_add((GSourceFunc) icon_grid_placement, ig);
        }
    }

    RET();
}

/* Handler for "size-request" event on the icon grid element. */
static void icon_grid_element_size_request(GtkWidget * widget, GtkRequisition * requisition, IconGridElement * ige)
{
    ENTER;
    /* This is our opportunity to request space for the element. */
    IconGrid * ig = ige->ig;
    requisition->width = ig->allocated_child_width;
    requisition->height = ig->allocated_child_height;
    RET();
}

/* Handler for "size-request" event on the icon grid's container. */
static void icon_grid_size_request(GtkWidget * widget, GtkRequisition * requisition, IconGrid * ig)
{
    ENTER;
    /* This is our opportunity to request space for the layout container.
     * Compute the geometry.  Do not lay out children at this time to avoid a recursive loop. */
    icon_grid_geometry(ig, GEOMETRY_SIZE_REQUEST);

    requisition->width = ig->requisition_width;
    requisition->height = ig->requisition_height;

    ig->requisition_changed = FALSE;

    if ((ig->columns == 0) || (ig->rows == 0))
        gtk_widget_hide(ig->widget);	/* Necessary to get the plugin to disappear */
    else
        gtk_widget_show(ig->widget);
    RET();
}

/* Handler for "size-allocate" event on the icon grid's container. */
static void icon_grid_size_allocate(GtkWidget * widget, GtkAllocation * allocation, IconGrid * ig)
{
    ENTER;
    //g_print("[0x%x] icon_grid_size_allocate: %d %d %d %d\n", (int)ig, allocation->x, allocation->y, allocation->width, allocation->height);

    /* This is our notification that there is a resize of the entire panel.
     * Compute the geometry and recompute layout if the geometry changed. */
    icon_grid_geometry(ig, GEOMETRY_SIZE_ALLOCATED);
    RET();
}

/* Initiate a resize. */
static void icon_grid_demand_resize(IconGrid * ig)
{
    ENTER;
    ig->children_changed = TRUE;
    icon_grid_geometry(ig, GEOMETRY_DEMAND_RESIZE);
    RET();
}

/* Establish an icon grid in a specified container widget.
 * The icon grid manages the contents of the container.
 * The orientation, geometry of the elements, and spacing can be varied.  All elements are the same size. */
IconGrid * icon_grid_new(
    Panel * panel, GtkWidget * container,
    GtkOrientation orientation, gint child_width, gint child_height, gint spacing, gint border, gint target_dimension)
{
    ENTER;

    /* Create a structure representing the icon grid and collect the parameters. */
    IconGrid * ig = g_new0(IconGrid, 1);
    ig->panel = panel;
    ig->container = container;
    ig->orientation = orientation;
    ig->child_width = child_width;
    ig->allocated_child_width = 0;
    ig->allocated_child_height = 0;
    ig->expand = FALSE;
    ig->child_height = child_height;
    ig->spacing = spacing;
    ig->border = border;
    ig->target_dimension = target_dimension;
    ig->deferred = 0;
    ig->deferred_resize = 0;
    ig->requisition_changed = TRUE;

    /* Create a layout container. */
    ig->widget = gtk_fixed_new();
    GTK_WIDGET_SET_FLAGS(ig->widget, GTK_NO_WINDOW);
    gtk_widget_set_redraw_on_allocate(ig->widget, FALSE);
    gtk_container_add(GTK_CONTAINER(ig->container), ig->widget);
    gtk_widget_show(ig->widget);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(ig->widget), "size-request", G_CALLBACK(icon_grid_size_request), (gpointer) ig);
    g_signal_connect(G_OBJECT(container), "size-request", G_CALLBACK(icon_grid_size_request), (gpointer) ig);
    g_signal_connect(G_OBJECT(container), "size-allocate", G_CALLBACK(icon_grid_size_allocate), (gpointer) ig);

    RET(ig);
}

/* Add an icon grid element and establish its initial visibility. */
void icon_grid_add(IconGrid * ig, GtkWidget * child, gboolean visible)
{
    ENTER;

    /* Create and initialize a structure representing the child. */
    IconGridElement * ige = g_new0(IconGridElement, 1);
    ige->ig = ig;
    ige->widget = child;
    ige->visible = visible;

    /* Insert at the tail of the child list.  This keeps the graphics in the order they were added. */
    if (ig->child_list == NULL)
        ig->child_list = ige;
    else
    {
        IconGridElement * ige_cursor;
        for (ige_cursor = ig->child_list; ige_cursor->flink != NULL; ige_cursor = ige_cursor->flink) ;
        ige_cursor->flink = ige;
    }

    /* Add the widget to the layout container. */
    if (visible)
        gtk_widget_show(ige->widget);
    gtk_fixed_put(GTK_FIXED(ig->widget), ige->widget, 0, 0);
    g_signal_connect(G_OBJECT(child), "size-request", G_CALLBACK(icon_grid_element_size_request), (gpointer) ige);

    /* Do a relayout. */
    icon_grid_demand_resize(ig);

    RET();
}

extern void icon_grid_set_expand(IconGrid * ig, gboolean expand)
{
    ig->expand = expand;
}

/* Remove an icon grid element. */
void icon_grid_remove(IconGrid * ig, GtkWidget * child)
{
    ENTER;

    IconGridElement * ige_pred = NULL;
    IconGridElement * ige;
    for (ige = ig->child_list; ige != NULL; ige_pred = ige, ige = ige->flink)
    {
        if (ige->widget == child)
        {
            /* The child is found.  Remove from child list and layout container. */
            gtk_widget_hide(ige->widget);
            gtk_container_remove(GTK_CONTAINER(ig->widget), ige->widget);

            if (ige_pred == NULL)
                ig->child_list = ige->flink;
            else
                ige_pred->flink = ige->flink;

            /* Do a relayout. */
            icon_grid_demand_resize(ig);
            break;
        }
    }

    RET();
}

extern void icon_grid_place_child_after(IconGrid * ig, GtkWidget * child, GtkWidget * after)
{
    /* Search 'after' element */
    IconGridElement * ige_after = NULL;
    if (after)
    {
        for (ige_after = ig->child_list; ige_after != NULL; ige_after = ige_after->flink)
        {
            if (ige_after->widget == after)
                break;
        }

        if (!ige_after)
            return;

        if (ige_after->flink && ige_after->flink->widget == child)
            return;
    }

    /* Remove the child from its current position. */
    IconGridElement * ige_pred = NULL;
    IconGridElement * ige;
    for (ige = ig->child_list; ige != NULL; ige_pred = ige, ige = ige->flink)
    {
        if (ige->widget == child)
        {
            if (ige_pred == NULL) {
                if (ige_after == NULL)
                    return;
                else
                    ig->child_list = ige->flink;
            } else {
                ige_pred->flink = ige->flink;
            }
            break;
        }
    }

    if (ige_after) {
       ige->flink = ige_after->flink;
       ige_after->flink = ige;
    } else {
        ige->flink = ig->child_list;
        ig->child_list = ige;
    }

    if (ige->visible)
        icon_grid_demand_resize(ig);

}

/* Reorder an icon grid element. */
extern void icon_grid_reorder_child(IconGrid * ig, GtkWidget * child, gint position)
{
    /* Remove the child from its current position. */
    IconGridElement * ige_pred = NULL;
    IconGridElement * ige;
    for (ige = ig->child_list; ige != NULL; ige_pred = ige, ige = ige->flink)
    {
        if (ige->widget == child)
        {
            if (ige_pred == NULL)
                ig->child_list = ige->flink;
            else
                ige_pred->flink = ige->flink;
            break;
        }
    }

    /* If the child was found, insert it at the new position. */
    if (ige != NULL)
    {
        if (ig->child_list == NULL)
        {
            ige->flink = NULL;
            ig->child_list = ige;
        }
        else if (position == 0)
        {
            ige->flink = ig->child_list;
            ig->child_list = ige;
        }
        else
            {
            int local_position = position - 1;
            IconGridElement * ige_pred;
            for (
              ige_pred = ig->child_list;
              ((ige_pred != NULL) && (local_position > 0));
              local_position -= 1, ige_pred = ige_pred->flink) ;
            ige->flink = ige_pred->flink;
            ige_pred->flink = ige;
            }

        /* Do a relayout. */
        if (ige->visible)
            icon_grid_demand_resize(ig);
    }
}

/* Change the geometry of an icon grid. */
void icon_grid_set_geometry(IconGrid * ig,
    GtkOrientation orientation, gint child_width, gint child_height, gint spacing, gint border, gint target_dimension)
{
    if (
        ig->orientation == orientation &&
        ig->child_width == child_width &&
        ig->child_height == child_height &&
        ig->spacing == spacing &&
        ig->border == border &&
        ig->target_dimension == target_dimension
    ) return;

    ig->orientation = orientation;
    ig->child_width = child_width;
    ig->child_height = child_height;
    ig->spacing = spacing;
    ig->border = border;
    ig->target_dimension = target_dimension;
    icon_grid_demand_resize(ig);
}

/* Change the visibility of an icon grid element. */
void icon_grid_set_visible(IconGrid * ig, GtkWidget * child, gboolean visible)
{
    ENTER;

    IconGridElement * ige;
    for (ige = ig->child_list; ige != NULL; ige = ige->flink)
    {
        if (ige->widget == child)
        {
            if (ige->visible != visible)
            {
                //g_print("[0x%x] 0x%x %s\n", (int)ig, (int)ige, visible ? "shown" : "hidden");

                /* Found, and the visibility changed.  Do a relayout. */
                ige->visible = visible;
                if (ig->deferred < 1) {
                    if ( ! ige->visible)
                        gtk_widget_hide(ige->widget);
                    icon_grid_demand_resize(ig);
                } else {
                    ig->deferred_resize = 1;
                    ige->deferred_hide = 1;
                }
            }
            break;
        }
    }

    RET();
}

/* Deallocate the icon grid structures. */
void icon_grid_free(IconGrid * ig)
{
    ENTER;

    if (ig->placement_idle_cb)
    {
        g_source_remove(ig->placement_idle_cb);
    }

    /* Hide the layout container. */
    if (ig->widget != NULL)
        gtk_widget_hide(ig->widget);

    /* Free all memory. */
    IconGridElement * ige = ig->child_list;
    while (ige != NULL)
    {
        IconGridElement * ige_succ = ige->flink;
        g_free(ige);
        ige = ige_succ;
    }
    g_free(ig);

    RET();
}

extern void icon_grid_defer_updates(IconGrid * ig)
{
    ig->deferred++;
}

extern void icon_grid_resume_updates(IconGrid * ig)
{
    ig->deferred--;
    if (ig->deferred < 1)
    {
        ig->deferred = 0;
        if (ig->deferred_resize)
            ig->deferred_resize = 0,
            icon_grid_demand_resize(ig);
    }
}

