/**
 * Copyright (C) 2002-2003 Anatoly Asviyan <aanatoly@users.sf.net>
 *                         Joe MacDonald   <joe@deserted.net>
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>

#include <sde-utils-jansson.h>

#define PLUGIN_PRIV_TYPE PagerPlugin

#include <waterline/symbol_visibility.h>
#include <waterline/global.h>
#include <waterline/panel.h>
#include <waterline/misc.h>
#include <waterline/plugin.h>
#include <waterline/x11_utils.h>
#include <waterline/x11_wrappers.h>

struct _task;
struct _desk;
struct _pager;

#define ALL_DESKTOPS   0xFFFFFFFF /* 64-bit clean */
#define BORDER_WIDTH   2

/* Structure representing a "task", an open window. */
typedef struct _task {
    struct _pager * pager;
    struct _task * task_flink; /* Forward link of task list */
    Window win;                /* X window ID */
    int x;                     /* Geometry as reported by X server */
    int y;
    guint w;
    guint h;
    int stacking;              /* Stacking order as reported by NET_WM_CLIENT_STACKING */
    int desktop;               /* Desktop that contains task */
    int ws;                    /* WM_STATE value */
    NetWMState nws;            /* NET_WM_STATE value */
    NetWMWindowType nwwt;      /* NET_WM_WINDOW_TYPE value */
    guint focused : 1;         /* True if window has focus */
    guint present_in_client_list : 1; /* State during WM_CLIENT_LIST processing to detect deletions */
    gboolean visible_on_pixmap;
} PagerTask;

/* Structure representing a desktop. */
typedef struct _desk {
    struct _pager * pg;        /* Back pointer to plugin context */
    GtkWidget * da;            /* Drawing area */
    GdkPixmap * pixmap;        /* Pixmap to be drawn on drawing area */
    int desktop_number;        /* Desktop number */
    gboolean dirty;            /* True if needs to be recomputed */
    gfloat scale_x;            /* Horizontal scale factor */
    gfloat scale_y;            /* Vertical scale factor */
} PagerDesktop;

/* Private context for pager plugin. */
typedef struct _pager {
    Plugin * plugin;                /* Back pointer to plugin */
    IconGrid * icon_grid;           /* Container widget */
    int desk_extent;                /* Extent of desks vector */
    PagerDesktop * * desks;         /* Vector of desktop structures */
    guint number_of_desktops;       /* Number of desktops, from NET_WM_NUMBER_OF_DESKTOPS */
    guint current_desktop;          /* Current desktop, from NET_WM_CURRENT_DESKTOP */
    gfloat aspect_ratio;            /* Aspect ratio of screen image */
    int client_count;               /* Count of tasks in stacking order */
    PagerTask * * tasks_in_stacking_order; /* Vector of tasks in stacking order */
    PagerTask * task_list;          /* Tasks in window ID order */
    PagerTask * focused_task;       /* Task that has focus */
} PagerPlugin;

static gboolean task_is_visible(PagerTask * tk);
static PagerTask * task_lookup(PagerPlugin * pg, Window win);
static void task_delete(PagerTask * tk, gboolean unlink);
static void task_get_geometry(PagerTask * tk);
static void task_update_pixmap(PagerTask * tk, PagerDesktop * d);
static void desktop_set_dirty(PagerDesktop * d);
static void pager_set_dirty_all_desktops(PagerPlugin * pg);
static void task_set_desktop_dirty(PagerTask * tk);
static gboolean desktop_configure_event(GtkWidget * widget, GdkEventConfigure * event, PagerDesktop * d);
static gboolean desktop_expose_event(GtkWidget * widget, GdkEventExpose * event, PagerDesktop * d);
static gboolean desktop_scroll_event(GtkWidget * widget, GdkEventScroll * event, PagerDesktop * d);
static gboolean desktop_button_press_event(GtkWidget * widget, GdkEventButton * event, PagerDesktop * d);
static void desktop_new(PagerPlugin * pg, int desktop_number);
static void desktop_free(PagerPlugin * pg, int desktop_number);
static void pager_property_notify_event(PagerPlugin * p, XEvent * ev);
static void pager_configure_notify_event(PagerPlugin * pg, XEvent * ev);
static GdkFilterReturn pager_event_filter(XEvent * xev, GdkEvent * event, PagerPlugin * pg);
static void pager_net_active_window(FbEv * ev, PagerPlugin * pg);
static void pager_net_desktop_names(FbEv * ev, PagerPlugin * pg);
static void pager_net_number_of_desktops(FbEv * ev, PagerPlugin * pg);
static void pager_net_client_list_stacking(FbEv * ev, PagerPlugin * pg);
static int pager_constructor(Plugin * plug);
static void pager_destructor(Plugin * p);
static void pager_panel_configuration_changed(Plugin * p);

/*****************************************************************
 * Task Management Routines                                      *
 *****************************************************************/

/* Determine if a task is visible. */
static gboolean task_is_visible(PagerTask * tk)
{
    return ( ! ((tk->nws.hidden) || (tk->nws.skip_pager) || (tk->nwwt.dock) || (tk->nwwt.desktop)));
}

/* Look up a task in the task list. */
static PagerTask * task_lookup(PagerPlugin * pg, Window win)
{
    PagerTask * tk;
    for (tk = pg->task_list; tk != NULL; tk = tk->task_flink)
    {
        if (tk->win == win)
            return tk;
        if (tk->win > win)
            break;
    }
    return NULL;
}

/* Delete a task and optionally unlink it from the task list. */
static void task_delete(PagerTask * tk, gboolean unlink)
{
    PagerPlugin * pg = tk->pager;
    task_set_desktop_dirty(tk);

    /* If we think this task had focus, remove that. */
    if (pg->focused_task == tk)
        pg->focused_task = NULL;

    /* If requested, unlink the task from the task list.
     * If not requested, the caller will do this. */
    if (unlink)
    {
        if (pg->task_list == tk)
            pg->task_list = tk->task_flink;
        else
        {
            /* Locate the task and its predecessor in the list and then remove it.  For safety, ensure it is found. */
            PagerTask * tk_pred = NULL;
            PagerTask * tk_cursor;
            for (
              tk_cursor = pg->task_list;
              ((tk_cursor != NULL) && (tk_cursor != tk));
              tk_pred = tk_cursor, tk_cursor = tk_cursor->task_flink) ;
            if (tk_cursor == tk)
                tk_pred->task_flink = tk->task_flink;
        }
    }

    /* Deallocate the task structure. */
    g_free(tk);
}

/* Get the geometry of a task window in screen coordinates. */
static void task_get_geometry(PagerTask * tk)
{
    /* Install an error handler that ignores BadWindow and BadDrawable.
     * We frequently get a ConfigureNotify event on deleted windows. */
    XErrorHandler previous_error_handler = XSetErrorHandler(panel_handle_x_error_swallow_BadWindow_BadDrawable);

    XWindowAttributes win_attributes;
    if (XGetWindowAttributes(wtl_x11_display(), tk->win, &win_attributes))
    {
        Window unused_win;
        int rx, ry;
        XTranslateCoordinates(wtl_x11_display(), tk->win, win_attributes.root,
              - win_attributes.border_width,
              - win_attributes.border_width,
              &rx, &ry, &unused_win);
        tk->x = rx;
        tk->y = ry;
        tk->w = win_attributes.width;
        tk->h = win_attributes.height;
    }
    else
    {
        Window unused_win;
        guint unused;
        if ( ! XGetGeometry(wtl_x11_display(), tk->win,
            &unused_win, &tk->x, &tk->y, &tk->w, &tk->h, &unused, &unused))
        {
            tk->x = tk->y = tk->w = tk->h = 2;
        }
    }

    XSetErrorHandler(previous_error_handler);
}

/* Draw the representation of a task's window on the backing pixmap. */
static void task_update_pixmap(PagerTask * tk, PagerDesktop * d)
{
    if (!d->pixmap)
        return;

    tk->visible_on_pixmap = task_is_visible(tk);

    if (!tk->visible_on_pixmap)
        return;

    if ((tk->desktop != ALL_DESKTOPS) && (tk->desktop != d->desktop_number))
        return;

    /* Scale the representation of the window to the drawing area. */
    int x = (gfloat) tk->x * d->scale_x;
    int y = (gfloat) tk->y * d->scale_y;
    int w = (gfloat) tk->w * d->scale_x;
    int h = ((tk->nws.shaded) ? 3 : (gfloat) tk->h * d->scale_y);
    if ((w >= 3) && (h >= 3))
    {
        /* Draw the window representation and a border. */
        GtkWidget * widget = GTK_WIDGET(d->da);

        cairo_t * cr = gdk_cairo_create(d->pixmap);

        cairo_set_line_width (cr, 1.0);
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

        if (d->pg->focused_task == tk)
            gdk_cairo_set_source_color(cr, &widget->style->bg[GTK_STATE_SELECTED]);
        else
            gdk_cairo_set_source_color(cr, &widget->style->bg[GTK_STATE_NORMAL]);

        cairo_rectangle(cr, x + 0.5, y + 0.5, w, h);
        cairo_fill(cr);

        if (d->pg->focused_task == tk)
            gdk_cairo_set_source_color(cr, &widget->style->fg[GTK_STATE_SELECTED]);
        else
            gdk_cairo_set_source_color(cr, &widget->style->fg[GTK_STATE_NORMAL]);

        cairo_rectangle(cr, x + 0.5, y + 0.5, w, h);
        cairo_stroke(cr);

        cairo_destroy(cr);

    }
}

/*****************************************************************
 * Desk Functions                                                *
 *****************************************************************/

/* Mark a specified desktop for redraw. */
static void desktop_set_dirty(PagerDesktop * d)
{
    d->dirty = TRUE;
    gtk_widget_queue_draw(d->da);
}

/* Mark all desktops for redraw. */
static void pager_set_dirty_all_desktops(PagerPlugin * pg)
{
    int i;
    for (i = 0; i < pg->number_of_desktops; i++)
        desktop_set_dirty(pg->desks[i]);
}

/* Mark the desktop on which a specified window resides for redraw. */
static void task_set_desktop_dirty(PagerTask * tk)
{
    PagerPlugin * pg = tk->pager;

    if (tk->visible_on_pixmap || task_is_visible(tk))
    {
        if (tk->desktop < pg->number_of_desktops)
            desktop_set_dirty(pg->desks[tk->desktop]);
        else
            pager_set_dirty_all_desktops(pg);
    }
}

/* Handler for configure_event on drawing area. */
static gboolean desktop_configure_event(GtkWidget * widget, GdkEventConfigure * event, PagerDesktop * d)
{
    /* Allocate pixmap and statistics buffer without border pixels. */
    int new_pixmap_width = widget->allocation.width;
    int new_pixmap_height = widget->allocation.height;
    if ((new_pixmap_width > 0) && (new_pixmap_height > 0))
    {
        /* Allocate a new pixmap of the allocated size. */
        if (d->pixmap != NULL)
            g_object_unref(d->pixmap);
        d->pixmap = gdk_pixmap_new(widget->window, new_pixmap_width, new_pixmap_height, -1);

        /* Compute the horizontal and vertical scale factors, and mark the desktop for redraw. */
        d->scale_y = (gfloat) widget->allocation.height / (gfloat) gdk_screen_height();
        d->scale_x = (gfloat) widget->allocation.width  / (gfloat) gdk_screen_width();
        desktop_set_dirty(d);
     }

    /* Resize to optimal size. */
    gtk_widget_set_size_request(widget,
        (plugin_get_icon_size(d->pg->plugin) - BORDER_WIDTH * 2) * d->pg->aspect_ratio,
        plugin_get_icon_size(d->pg->plugin) - BORDER_WIDTH * 2);
    return FALSE;
}

/* Handler for expose_event on drawing area. */
static gboolean desktop_expose_event(GtkWidget * widget, GdkEventExpose * event, PagerDesktop * d)
{
    if (d->pixmap != NULL)
    {
        /* Recompute the pixmap if needed. */
        if (d->dirty)
        {
            d->dirty = FALSE;
            PagerPlugin * pg = d->pg;

            /* Erase the pixmap. */
            if (d->pixmap != NULL)
            {
                GtkWidget * widget = GTK_WIDGET(d->da);

                cairo_t * cr = gdk_cairo_create(d->pixmap);

                if (d->desktop_number == d->pg->current_desktop)
                    gdk_cairo_set_source_color(cr, &widget->style->dark[GTK_STATE_SELECTED]);
                else
                    gdk_cairo_set_source_color(cr, &widget->style->dark[GTK_STATE_NORMAL]);

                cairo_rectangle(cr, 0, 0, widget->allocation.width, widget->allocation.height);
                cairo_fill(cr);

                cairo_destroy(cr);
            }

            /* Draw tasks onto the pixmap. */
            int j;
            for (j = 0; j < pg->client_count; j++)
                task_update_pixmap(pg->tasks_in_stacking_order[j], d);
        }

        /* Draw the requested part of the pixmap onto the drawing area. */
        gdk_draw_drawable(widget->window,
              widget->style->fg_gc[gtk_widget_get_state(widget)],
              d->pixmap,
              event->area.x, event->area.y,
              event->area.x, event->area.y,
              event->area.width, event->area.height);
    }
    return FALSE;
}

/* Handler for "scroll-event" on drawing area. */
static gboolean desktop_scroll_event(GtkWidget * widget, GdkEventScroll * event, PagerDesktop * d)
{
    /* Compute the new desktop from the scroll direction, wrapping at either extreme. */
    int current_desktop = d->pg->current_desktop;
    if ((event->direction == GDK_SCROLL_DOWN) || (event->direction == GDK_SCROLL_RIGHT))
    {
        current_desktop += 1;
        if (current_desktop >= d->pg->number_of_desktops)
            current_desktop = 0;
    }
    else
    {
        current_desktop -= 1;
        if (current_desktop < 0)
            current_desktop = d->pg->number_of_desktops - 1;
    }

    /* Ask the window manager to make the new desktop current. */
    Xclimsg(wtl_x11_root(), a_NET_CURRENT_DESKTOP, current_desktop, 0, 0, 0, 0);
    return TRUE;
}

/* Handler for "button-press-event" on drawing area. */
static gboolean desktop_button_press_event(GtkWidget * widget, GdkEventButton * event, PagerDesktop * d)
{
    /* Standard right-click handling. */
    if (plugin_button_press_event(widget, event, d->pg->plugin))
        return TRUE;

    /* Ask the window manager to make the new desktop current. */
    Xclimsg(wtl_x11_root(), a_NET_CURRENT_DESKTOP, d->desktop_number, 0, 0, 0, 0);
    return TRUE;
}

/* Allocate the structure and the graphic elements representing a desktop. */
static void desktop_new(PagerPlugin * pg, int desktop_number)
{

    /* Allocate and initialize structure. */
    PagerDesktop * d = pg->desks[desktop_number] = g_new0(PagerDesktop, 1);
    d->pg = pg;
    d->desktop_number = desktop_number;

    /* Allocate drawing area. */
    d->da = gtk_drawing_area_new();

    icon_grid_add(pg->icon_grid, d->da, TRUE);
    gtk_widget_add_events (d->da, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(d->da), "expose_event", G_CALLBACK(desktop_expose_event), (gpointer) d);
    g_signal_connect(G_OBJECT(d->da), "configure_event", G_CALLBACK(desktop_configure_event), (gpointer) d);
    g_signal_connect(G_OBJECT(d->da), "scroll-event", G_CALLBACK(desktop_scroll_event), (gpointer) d);
    g_signal_connect(G_OBJECT(d->da), "button_press_event", G_CALLBACK(desktop_button_press_event), (gpointer) d);

    /* Show the widget. */
    gtk_widget_show(d->da);
}

/* Free the structure representing a desktop. */
static void desktop_free(PagerPlugin * pg, int desktop_number)
{
    PagerDesktop * d = pg->desks[desktop_number];

    g_signal_handlers_disconnect_by_func(G_OBJECT(d->da), desktop_expose_event, d);
    g_signal_handlers_disconnect_by_func(G_OBJECT(d->da), desktop_configure_event, d);
    g_signal_handlers_disconnect_by_func(G_OBJECT(d->da), desktop_scroll_event, d);
    g_signal_handlers_disconnect_by_func(G_OBJECT(d->da), desktop_button_press_event, d);

    icon_grid_remove(pg->icon_grid, d->da);

    if (d->pixmap != NULL)
        g_object_unref(d->pixmap);

    g_free(d);
}

/*****************************************************************
 * Pager Functions                                               *
 *****************************************************************/

/* Handle PropertyNotify event.
 * http://tronche.com/gui/x/icccm/
 * http://standards.freedesktop.org/wm-spec/wm-spec-1.4.html */
static void pager_property_notify_event(PagerPlugin * pg, XEvent * ev)
{
    /* State may be PropertyNewValue, PropertyDeleted. */
    if (((XPropertyEvent*) ev)->state == PropertyNewValue)
    {
        Atom at = ev->xproperty.atom;
        Window win = ev->xproperty.window;
        if (win != wtl_x11_root())
        {
            /* Look up task structure by X window handle. */
            PagerTask * tk = task_lookup(pg, win);
            if (tk != NULL)
            {
                /* Install an error handler that ignores BadWindow.
                 * We frequently get a PropertyNotify event on deleted windows. */
                XErrorHandler previous_error_handler = XSetErrorHandler(panel_handle_x_error_swallow_BadWindow_BadDrawable);

                /* Dispatch on atom. */
                if (at == aWM_STATE)
                {
                    /* Window changed state. */
                    tk->ws = wtl_x11_get_wm_state(tk->win);
                    task_set_desktop_dirty(tk);
                }
                else if (at == a_NET_WM_STATE)
                {
                    /* Window changed EWMH state. */
                    wtl_x11_get_net_wm_state(tk->win, &tk->nws);
                    task_set_desktop_dirty(tk);
                }
                else if (at == a_NET_WM_DESKTOP)
                {
                    /* Window changed desktop.
                     * Mark both old and new desktops for redraw. */
                    task_set_desktop_dirty(tk);
                    tk->desktop = wtl_x11_get_net_wm_desktop(tk->win);
                    task_set_desktop_dirty(tk);
                }

                XSetErrorHandler(previous_error_handler);
            }
        }
    }
}

/* Handle ConfigureNotify event. */
static void pager_configure_notify_event(PagerPlugin * pg, XEvent * ev)
{
    Window win = ev->xconfigure.window;
    PagerTask * tk = task_lookup(pg, win);
    if (tk != NULL)
    {
        task_get_geometry(tk);
        task_set_desktop_dirty(tk);
    }
}

/* GDK event filter. */
static GdkFilterReturn pager_event_filter(XEvent * xev, GdkEvent * event, PagerPlugin * pg)
{
    /* Look for PropertyNotify and ConfigureNotify events and update state. */
    if (xev->type == PropertyNotify)
        pager_property_notify_event(pg, xev);
    else if (xev->type == ConfigureNotify)
        pager_configure_notify_event(pg, xev);
    return GDK_FILTER_CONTINUE;
}

/*****************************************************************
 * Netwm/WM Interclient Communication                            *
 *****************************************************************/

/* Handler for "active-window" event from root window listener. */
static void pager_net_active_window(FbEv * ev, PagerPlugin * pg)
{
    Window * focused_window = wtl_x11_get_xa_property(wtl_x11_root(), a_NET_ACTIVE_WINDOW, XA_WINDOW, 0);
    if (focused_window != NULL)
    {
        PagerTask * tk = task_lookup(pg, *focused_window);
        if (tk != pg->focused_task)
        {
            /* Focused task changed.  Redraw both old and new. */
            if (pg->focused_task != NULL)
                task_set_desktop_dirty(pg->focused_task);
            pg->focused_task = tk;
            if (tk != NULL)
                task_set_desktop_dirty(tk);
        }
        XFree(focused_window);
    }
    else
    {
        /* Focused task disappeared.  Redraw old. */
        if (pg->focused_task != NULL)
        {
            task_set_desktop_dirty(pg->focused_task);
            pg->focused_task = NULL;
        }
    }
}

/* Handler for desktop_name event from window manager. */
static void pager_net_desktop_names(FbEv * fbev, PagerPlugin * pg)
{
    /* Get the NET_DESKTOP_NAMES property. */
    int number_of_desktop_names;
    char * * desktop_names;
    desktop_names = wtl_x11_get_utf8_property_list(wtl_x11_root(), a_NET_DESKTOP_NAMES, &number_of_desktop_names);

    /* Loop to copy the desktop names to the vector of labels.
     * If there are more desktops than labels, label the extras with a decimal number. */
    int i;
    for (i = 0; ((desktop_names != NULL) && (i < MIN(pg->number_of_desktops, number_of_desktop_names))); i++)
        gtk_widget_set_tooltip_text(pg->desks[i]->da, desktop_names[i]);
    for ( ; i < pg->number_of_desktops; i++)
    {
        char temp[10];
        sprintf(temp, "%d", i + 1);
        gtk_widget_set_tooltip_text(pg->desks[i]->da, temp);
    }

    /* Free the property. */
    if (desktop_names != NULL)
        g_strfreev(desktop_names);
}

/* Handler for "current-desktop" event from root window listener. */
static void pager_net_current_desktop(FbEv * ev, PagerPlugin * pg)
{
    desktop_set_dirty(pg->desks[pg->current_desktop]);
    pg->current_desktop = wtl_x11_get_net_current_desktop();
    if (pg->current_desktop >= pg->number_of_desktops)
        pg->current_desktop = 0;
    desktop_set_dirty(pg->desks[pg->current_desktop]);
}


/* Handler for "number-of-desktops" event from root window listener.
 * Also used to initialize plugin. */
static void pager_net_number_of_desktops(FbEv * ev, PagerPlugin * pg)
{
    /* Get existing values. */
    int number_of_desktops = pg->number_of_desktops;

    /* Get the correct number of desktops. */
    pg->number_of_desktops = wtl_x11_get_net_number_of_desktops();
    if (pg->number_of_desktops < 1)
        pg->number_of_desktops = 1;

    /* Reallocate the structure if necessary. */
    if (pg->number_of_desktops > pg->desk_extent)
    {
        PagerDesktop * * new_desks = g_new(PagerDesktop *, pg->number_of_desktops);
        if (pg->desks != NULL)
        {
            memcpy(new_desks, pg->desks, pg->desk_extent * sizeof(PagerDesktop *));
            g_free(pg->desks);
        }
        pg->desks = new_desks;
        pg->desk_extent = pg->number_of_desktops;
    }

    /* Reconcile the current desktop number. */
    pg->current_desktop = wtl_x11_get_net_current_desktop();
    if (pg->current_desktop >= pg->number_of_desktops)
        pg->current_desktop = 0;

    /* Reconcile the old and new number of desktops. */
    int difference = pg->number_of_desktops - number_of_desktops;
    if (difference != 0)
    {
        if (difference < 0)
        {
            /* If desktops were deleted, then delete their maps also. */
            int i;
            for (i = pg->number_of_desktops; i < number_of_desktops; i++)
                desktop_free(pg, i);
        }
        else
        {
            /* If desktops were added, then create their maps also. */
            int i;
            for (i = number_of_desktops; i < pg->number_of_desktops; i++)
                desktop_new(pg, i);
        }
    }

    /* Refresh the client list. */
    pager_net_client_list_stacking(NULL, pg);
}

/* Handler for "net-client-list-stacking" event from root window listener. */
static void pager_net_client_list_stacking(FbEv * ev, PagerPlugin * pg)
{
    /* Get the NET_CLIENT_LIST_STACKING property. */
    Window * client_list = wtl_x11_get_xa_property(wtl_x11_root(), a_NET_CLIENT_LIST_STACKING, XA_WINDOW, &pg->client_count);
    g_free(pg->tasks_in_stacking_order);
    /* g_new returns NULL if if n_structs == 0 */
    pg->tasks_in_stacking_order = g_new(PagerTask *, pg->client_count);

    if (client_list != NULL)
    {
        /* Loop over client list, correlating it with task list.
         * Also generate a vector of task pointers in stacking order. */
        int i;
        for (i = 0; i < pg->client_count; i++)
        {
            /* Search for the window in the task list.  Set up context to do an insert right away if needed. */
            PagerTask * tk_pred = NULL;
            PagerTask * tk_cursor;
            PagerTask * tk = NULL;
            for (tk_cursor = pg->task_list; tk_cursor != NULL; tk_pred = tk_cursor, tk_cursor = tk_cursor->task_flink)
            {
                if (tk_cursor->win == client_list[i])
                {
                    tk = tk_cursor;
                    break;
                }
                if (tk_cursor->win > client_list[i])
                    break;
            }

            /* Task is already in task list. */
            if (tk != NULL)
            {
                tk->present_in_client_list = TRUE;

                /* If the stacking position changed, redraw the desktop. */
                if (tk->stacking != i)
                {
                    tk->stacking = i;
                    task_set_desktop_dirty(tk);
                }
            }

            /* Task is not in task list. */
            else
            {
                /* Allocate and initialize new task structure. */
                tk = g_new0(PagerTask, 1);
                tk->pager = pg;
                tk->present_in_client_list = TRUE;
                tk->win = client_list[i];
                if (!wtl_x11_is_my_own_window(tk->win))
                    XSelectInput(wtl_x11_display(), tk->win, PropertyChangeMask | StructureNotifyMask);
                tk->ws = wtl_x11_get_wm_state(tk->win);
                tk->desktop = wtl_x11_get_net_wm_desktop(tk->win);
                wtl_x11_get_net_wm_state(tk->win, &tk->nws);
                wtl_x11_get_net_wm_window_type(tk->win, &tk->nwwt);
                task_get_geometry(tk);
                task_set_desktop_dirty(tk);

                /* Link the task structure into the task list. */
                if (tk_pred == NULL)
                {
                    tk->task_flink = pg->task_list;
                    pg->task_list = tk;
                }
                else
                {
                    tk->task_flink = tk_pred->task_flink;
                    tk_pred->task_flink = tk;
                }
            }
            pg->tasks_in_stacking_order[i] = tk;
        }
        XFree(client_list);
    }

    /* Remove windows from the task list that are not present in the NET_CLIENT_LIST_STACKING. */
    PagerTask * tk_pred = NULL;
    PagerTask * tk = pg->task_list;
    while (tk != NULL)
    {
        PagerTask * tk_succ = tk->task_flink;
        if (tk->present_in_client_list)
        {
            tk->present_in_client_list = FALSE;
            tk_pred = tk;
        }
        else
        {
            if (tk_pred == NULL)
                pg->task_list = tk_succ;
                else tk_pred->task_flink = tk_succ;
            task_delete(tk, FALSE);
        }
        tk = tk_succ;
    }
}

/* Plugin constructor. */
static int pager_constructor(Plugin * plug)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    PagerPlugin * pg = g_new0(PagerPlugin, 1);
    plugin_set_priv(plug, pg);
    pg->plugin = plug;

    /* Compute aspect ratio of screen image. */
    pg->aspect_ratio = (gfloat) gdk_screen_width() / (gfloat) gdk_screen_height();

    /* Allocate top level widget and set into Plugin widget pointer. */
    GtkWidget * pwid = gtk_event_box_new();
    plugin_set_widget(plug, pwid);
    GTK_WIDGET_SET_FLAGS(pwid, GTK_NO_WINDOW);
    gtk_container_set_border_width(GTK_CONTAINER(pwid), 0);

    /* Create an icon grid manager to manage the drawing areas within the container. */
    GtkOrientation bo = (plugin_get_orientation(plug) == ORIENT_HORIZ) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    pg->icon_grid = icon_grid_new(pwid, bo,
        (plugin_get_icon_size(plug) - BORDER_WIDTH * 2) * pg->aspect_ratio,
        plugin_get_icon_size(plug) - BORDER_WIDTH * 2,
        1, BORDER_WIDTH,
        panel_get_oriented_height_pixels(plugin_panel(plug)));

    //icon_grid_debug_output(pg->icon_grid, TRUE);

    /* Add GDK event filter. */
    gdk_window_add_filter(NULL, (GdkFilterFunc) pager_event_filter, pg);

    /* Connect signals to receive root window events and initialize root window properties. */
    g_signal_connect(G_OBJECT(fbev), "current_desktop", G_CALLBACK(pager_net_current_desktop), (gpointer) pg);
    g_signal_connect(G_OBJECT(fbev), "active_window", G_CALLBACK(pager_net_active_window), (gpointer) pg);
    g_signal_connect(G_OBJECT(fbev), "desktop_names", G_CALLBACK(pager_net_desktop_names), (gpointer) pg);
    g_signal_connect(G_OBJECT(fbev), "number_of_desktops", G_CALLBACK(pager_net_number_of_desktops), (gpointer) pg);
    g_signal_connect(G_OBJECT(fbev), "client_list_stacking", G_CALLBACK(pager_net_client_list_stacking), (gpointer) pg);

    /* Allocate per-desktop structures. */
    pager_net_number_of_desktops(fbev, pg);
    pager_net_desktop_names(fbev, pg);
    return 1;
}

/* Plugin destructor. */
static void pager_destructor(Plugin * p)
{
    PagerPlugin * pg = PRIV(p);

    icon_grid_to_be_removed(pg->icon_grid);

    /* Remove GDK event filter. */
    gdk_window_remove_filter(NULL, (GdkFilterFunc) pager_event_filter, pg);

    /* Remove root window signal handlers. */
    g_signal_handlers_disconnect_by_func(G_OBJECT(fbev), pager_net_current_desktop, pg);
    g_signal_handlers_disconnect_by_func(G_OBJECT(fbev), pager_net_active_window, pg);
    g_signal_handlers_disconnect_by_func(G_OBJECT(fbev), pager_net_number_of_desktops, pg);
    g_signal_handlers_disconnect_by_func(G_OBJECT(fbev), pager_net_client_list_stacking, pg);

    /* Deallocate task list. */
    while (pg->task_list != NULL)
        task_delete(pg->task_list, TRUE);

    /* Deallocate desktop structures. */
    int i;
    for (i = 0; i < pg->number_of_desktops; i += 1)
        desktop_free(pg, i);

    /* Deallocate all memory. */
    icon_grid_free(pg->icon_grid);
    g_free(pg->tasks_in_stacking_order);
    g_free(pg);
}

/* Callback when panel configuration changes. */
static void pager_panel_configuration_changed(Plugin * p)
{
    /* Reset the icon grid orientation. */
    PagerPlugin * pg = PRIV(p);
    GtkOrientation bo = (plugin_get_orientation(p) == ORIENT_HORIZ) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    icon_grid_set_geometry(pg->icon_grid, bo,
        (plugin_get_icon_size(p) - BORDER_WIDTH * 2) * pg->aspect_ratio,
        plugin_get_icon_size(p) - BORDER_WIDTH * 2,
        1, BORDER_WIDTH,
        panel_get_oriented_height_pixels(plugin_panel(p)));
}

/* Plugin descriptor. */
SYMBOL_PLUGIN_CLASS PluginClass pager_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "pager",
    name : N_("Desktop Pager"),
    version: VERSION,
    description : N_("Simple pager plugin"),
    category: PLUGIN_CATEGORY_WINDOW_MANAGEMENT,

    constructor : pager_constructor,
    destructor  : pager_destructor,
    panel_configuration_changed : pager_panel_configuration_changed
};
