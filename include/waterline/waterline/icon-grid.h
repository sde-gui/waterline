/**
 * Copyright (c) 2011-2013 Vadim Ushakov
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

#ifndef __WATERLINE__ICON_GRID_H
#define __WATERLINE__ICON_GRID_H

#include <gtk/gtk.h>

struct _icon_grid_element;
struct _icon_grid;

/* Representative of an icon grid element.  This is a widget to be packed into a rectangular grid whose size adapts to conditions. */
typedef struct _icon_grid_element {
    struct _icon_grid_element * flink;		/* Forward link */
    struct _icon_grid * ig;			/* Back pointer to IconGrid */
    GtkWidget * widget;				/* Customer's widget */
    gboolean visible;				/* True if widget is visible */
    gboolean deferred_hide;
    gboolean separator;
    gboolean being_removed;
} IconGridElement;

/* Representative of an icon grid.  This is a manager that packs widgets into a rectangular grid whose size adapts to conditions. */
typedef struct _icon_grid {
    IconGridElement * child_list;		/* List of icon grid elements */
    GtkWidget * container;			/* Container widget */
    GtkOrientation orientation;			/* Desired orientation */
    gint child_width;				/* Desired child width */
    gint child_height;				/* Desired child height */
    gint spacing;				/* Desired spacing between grid elements */
    gint border;				/* Desired border around grid elements */
    gint target_dimension;			/* Desired dimension perpendicular to orientation */
    gboolean expand;				/*  */
    gboolean actual_dimension;			/* True if container has been allocated space */
    gboolean children_changed;			/* True if icon grid element list changed */
    GtkWidget * widget;				/* Layout widget we use for packing */

    int rows;					/* Computed layout rows */
    int columns;				/* Computed layout columns */

    gboolean use_separators;
    int separator_size;
    int col_separators;
    int row_separators;

    int requisition_width;			/* Computed preferred width */
    int requisition_height;			/* Computed preferred height */
    gboolean requisition_changed;		/* True if preferred size has been changed since last size-request */

    int container_width;			/* Container's allocated width */
    int container_height;			/* Container's allocated height */
    int allocated_child_width;
    int allocated_child_height;
    int deferred;
    int deferred_resize;

    gboolean to_be_removed;

    guint placement_idle_cb;

    gboolean debug_output;
} IconGrid;

extern IconGrid * icon_grid_new(GtkWidget * container,
    GtkOrientation orientation, gint child_width, gint child_height, gint spacing, gint border, gint target_dimension);
						/* Create an icon grid */
extern void icon_grid_set_expand(IconGrid * ig, gboolean expand);
						/* Set the expand property */
extern void icon_grid_set_geometry(IconGrid * ig,
    GtkOrientation orientation, gint child_width, gint child_height, gint spacing, gint border, gint target_dimension);
						/* Change the geometry of an icon grid */
extern void icon_grid_add(IconGrid * ig, GtkWidget * child, gboolean visible);
						/* Add a child to the icon grid */
extern void icon_grid_remove(IconGrid * ig, GtkWidget * child);
						/* Remove a child from the icon grid */
extern void icon_grid_reorder_child(IconGrid * ig, GtkWidget * child, gint position);
						/* Reorder the position of a child in the icon grid */
extern void icon_grid_place_child_after(IconGrid * ig, GtkWidget * child, GtkWidget * after);
						/*  */
extern void icon_grid_set_visible(IconGrid * ig, GtkWidget * child, gboolean visible);
						/* Set the visibility of a child in the icon grid */
extern void icon_grid_free(IconGrid * ig);	/* Free the icon grid */

extern void icon_grid_defer_updates(IconGrid * ig);
extern void icon_grid_resume_updates(IconGrid * ig);

extern void icon_grid_to_be_removed(IconGrid * ig);

extern void icon_grid_set_separator(IconGrid * ig, GtkWidget * child, gboolean separator);
extern void icon_grid_use_separators(IconGrid * ig, gboolean use_separators);
extern void icon_grid_set_separator_size(IconGrid * ig, int separator_size);

extern void icon_grid_debug_output(IconGrid * ig, gboolean debug_output);

#endif
