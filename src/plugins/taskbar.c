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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

#include "panel.h"
#include "misc.h"
#include "plugin.h"
#include "icon.xpm"
#include "gtkbar.h"
#include "icon-grid.h"

/*
 * 2006.09.10 modified by Hong Jen Yee (PCMan) pcman.tw (AT) gmail.com
 * Following features are added:
 * 1. Add XUrgencyHint support. (Flashing task bar buttons, can be disabled)
 * 2. Raise window when files get dragged over taskbar buttons.
 * 3. Add Restore & Maximize menu items to popup menu of task bar buttons.
 */

//#define DEBUG

#include "dbg.h"

struct _taskbar;
struct _task_class;
struct _task;

/******************************************************************************/

enum TASKBAR_MODE {
    MODE_CLASSIC,
    MODE_GROUP,
    MODE_SINGLE_WINDOW
};

enum {
    SHOW_ICONS,
    SHOW_TITLES,
    SHOW_BOTH
};

enum TASKBAR_ACTION {
    ACTION_NONE,
    ACTION_MENU,
    ACTION_CLOSE,
    ACTION_RAISEICONIFY,
    ACTION_ICONIFY,
    ACTION_MAXIMIZE,
    ACTION_SHADE,
    ACTION_UNDECORATE,
    ACTION_FULLSCREEN,
    ACTION_STICK,
    ACTION_SHOW_WINDOW_LIST,
    ACTION_SHOW_SIMILAR_WINDOW_LIST,
    ACTION_NEXT_WINDOW,
    ACTION_PREV_WINDOW,
    ACTION_NEXT_WINDOW_IN_CURRENT_GROUP,
    ACTION_PREV_WINDOW_IN_CURRENT_GROUP,
    ACTION_NEXT_WINDOW_IN_GROUP,
    ACTION_PREV_WINDOW_IN_GROUP
};

enum {
    GROUP_BY_NONE,
    GROUP_BY_CLASS,
    GROUP_BY_WORKSPACE,
    GROUP_BY_STATE
};

/******************************************************************************/

static pair show_pair[] = {
    { SHOW_ICONS, "Icons"},
    { SHOW_TITLES, "Titles"},
    { SHOW_BOTH, "Both"},
    { 0, NULL},
};

static pair mode_pair[] = {
    { MODE_CLASSIC, "Classic"},
    { MODE_GROUP, "Group"},
    { MODE_SINGLE_WINDOW, "SingleWindow"},
    { 0, NULL},
};

static pair action_pair[] = {
    { ACTION_NONE, "None"},
    { ACTION_MENU, "Menu"},
    { ACTION_CLOSE, "Close"},
    { ACTION_RAISEICONIFY, "RaiseIconify"},
    { ACTION_ICONIFY, "Iconify"},
    { ACTION_MAXIMIZE, "Maximize"},
    { ACTION_SHADE, "Shade"},
    { ACTION_UNDECORATE, "Undecorate"},
    { ACTION_FULLSCREEN, "Fullscreen"},
    { ACTION_STICK, "Stick"},
    { ACTION_SHOW_WINDOW_LIST, "ShowWindowList"},
    { ACTION_SHOW_SIMILAR_WINDOW_LIST, "ShowGroupedWindowList"},
    { ACTION_NEXT_WINDOW, "NextWindow"},
    { ACTION_PREV_WINDOW, "PrevWindow"},
    { ACTION_NEXT_WINDOW_IN_CURRENT_GROUP, "NextWindowInCurrentGroup"},
    { ACTION_PREV_WINDOW_IN_CURRENT_GROUP, "PrevWindowInCurrentGroup"},
    { ACTION_NEXT_WINDOW_IN_GROUP, "NextWindowInGroup"},
    { ACTION_PREV_WINDOW_IN_GROUP, "PrevWindowInGroup"},
    { 0, NULL},
};

static pair group_by_pair[] = {
    { GROUP_BY_NONE, "None" },
    { GROUP_BY_CLASS, "Class" },
    { GROUP_BY_WORKSPACE,  "Workspace" },
    { GROUP_BY_STATE,  "State" },
    { 0, NULL},
};

/******************************************************************************/

/* Structure representing a class.  This comes from WM_CLASS, and should identify windows that come from an application. */
typedef struct _task_class {
    struct _task_class * res_class_flink;	/* Forward link */
    char * res_class;				/* Class name */
    struct _task * res_class_head;		/* Head of list of tasks with this class */
    struct _task * visible_task;		/* Task that is visible in current desktop, if any */
    char * visible_name;			/* Name that will be visible for grouped tasks */
    int visible_count;				/* Count of tasks that are visible in current desktop */
    int timestamp;
    
    gboolean expand;
    gboolean manual_expand_state;
} TaskClass;

/* Structure representing a "task", an open window. */
typedef struct _task {
    struct _task * task_flink;			/* Forward link to next task in X window ID order */
    struct _taskbar * tb;			/* Back pointer to taskbar */
    Window win;					/* X window ID */
    char * name;				/* Taskbar label when normal, from WM_NAME or NET_WM_NAME */
    char * name_iconified;			/* Taskbar label when iconified */
    Atom name_source;				/* Atom that is the source of taskbar label */
    
    TaskClass * res_class;			/* Class, from WM_CLASS */
    struct _task * res_class_flink;		/* Forward link to task in same class */
    char * override_class_name;

    GtkWidget * button;				/* Button representing task in taskbar */
    GtkWidget * container;			/* Container for image, label and close button. */
    GtkWidget * image;				/* Icon for task, child of button */
    Atom image_source;				/* Atom that is the source of taskbar icon */
    GtkWidget * label;				/* Label for task, child of button */
    GtkWidget * button_close;			/* Close button */

    GtkAllocation button_alloc;
    guint adapt_to_allocated_size_idle_cb;
    guint update_icon_idle_cb;

    int desktop;				/* Desktop that contains task, needed to switch to it on Raise */
    guint flash_timeout;			/* Timer for urgency notification */
    unsigned int focused : 1;			/* True if window has focus */
    unsigned int iconified : 1;			/* True if window is iconified, from WM_STATE */
    unsigned int maximized : 1;			/* True if window is maximized, from WM_STATE */
    unsigned int urgency : 1;			/* True if window has an urgency hint, from WM_HINTS */
    unsigned int flash_state : 1;		/* One-bit counter to flash taskbar */
    unsigned int entered_state : 1;		/* True if cursor is inside taskbar button */
    unsigned int present_in_client_list : 1;	/* State during WM_CLIENT_LIST processing to detect deletions */

    int timestamp;

    int focus_timestamp;

    GtkWidget* click_on;
    
    int allocated_icon_size;
    int icon_size;
} Task;

/* Private context for taskbar plugin. */
typedef struct _taskbar {
    Plugin * plug;				/* Back pointer to Plugin */
    Task * task_list;				/* List of tasks to be displayed in taskbar */
    TaskClass * res_class_list;			/* Window class list */
    IconGrid * icon_grid;			/* Manager for taskbar buttons */

    int task_timestamp;

    GtkWidget * menu;				/* Popup menu for task control (Close, Raise, etc.) */
    GtkWidget * workspace_submenu;		/* Workspace submenu of the task control menu */
    GtkWidget * restore_menuitem;		/*  */
    GtkWidget * maximize_menuitem;		/*  */
    GtkWidget * iconify_menuitem;		/*  */
    GtkWidget * ungroup_menuitem;		/*  */
    GtkWidget * move_to_group_menuitem;		/*  */
    GtkWidget * expand_group_menuitem;		/*  */
    GtkWidget * shrink_group_menuitem;		/*  */
    GtkWidget * title_menuitem;			/*  */

    GtkWidget * group_menu;			/* Popup menu for grouping selection */
    GdkPixbuf * fallback_pixbuf;		/* Fallback task icon when none is available */
    char * * desktop_names;
    int number_of_desktop_names;
    int number_of_desktops;			/* Number of desktops, from NET_WM_NUMBER_OF_DESKTOPS */
    int current_desktop;			/* Current desktop, from NET_WM_CURRENT_DESKTOP */
    Task * focused;				/* Task that has focus */
    Task * focused_previous;			/* Task that had focus just before panel got it */
    Task * menutask;				/* Task for which popup menu is open */
    guint dnd_delay_timer;			/* Timer for drag and drop delay */
    int icon_size;				/* Size of task icons */
    int extra_size;

    int button1_action;                         /* User preference: left button action */
    int button2_action;                         /* User preference: middle button action */
    int button3_action;                         /* User preference: right button action */
    int scroll_up_action;                       /* User preference: scroll up action */
    int scroll_down_action;                     /* User preference: scroll down action */

    int shift_button1_action;                   /* User preference: shift + left button action */
    int shift_button2_action;                   /* User preference: shift + middle button action */
    int shift_button3_action;                   /* User preference: shift + right button action */
    int shift_scroll_up_action;                 /* User preference: shift + scroll up action */
    int shift_scroll_down_action;               /* User preference: shift + scroll down action */


    gboolean show_all_desks;			/* User preference: show windows from all desktops */
    gboolean show_mapped;			/* User preference: show mapped windows */
    gboolean show_iconified;			/* User preference: show iconified windows */
    gboolean tooltips;				/* User preference: show tooltips */
    int show_icons_titles;			/* User preference: show icons, titles */
    gboolean show_close_buttons;		/* User preference: show close button */
    
    gboolean use_urgency_hint;			/* User preference: windows with urgency will flash */
    gboolean flat_button;			/* User preference: taskbar buttons have visible background */
    
    int mode;                                   /* User preference: view mode */
    int group_shrink_threshold;                 /* User preference: threshold for shrinking grouped tasks into one button */
    int group_by;                               /* User preference: attr to group tasks by */
    gboolean manual_grouping;			/* User preference: manual grouping */
    gboolean expand_focused_group;		/* User preference: autoexpand group of focused window */

    int task_width_max;				/* Maximum width of a taskbar button in horizontal orientation */
    int spacing;				/* Spacing between taskbar buttons */

    gboolean use_net_active;			/* NET_WM_ACTIVE_WINDOW is supported by the window manager */
    gboolean net_active_checked;		/* True if use_net_active is valid */

    /* Effective config values, evaluated from "User preference" variables: */
    gboolean grouped_tasks;			/* Group task of the same class into single button. */
    gboolean single_window;			/* Show only current window button. */
    gboolean group_side_by_side;		/* Group task of the same class side by side. (value from last gui rebuild) */
    gboolean rebuild_gui;			/* Force gui rebuild (when configuration changed) */
    gboolean show_all_desks_prev_value;         /* Value of show_all_desks from last gui rebuild */
    gboolean show_icons;			/* Show icons */
    gboolean show_titles;			/* Show title labels */
    gboolean _show_close_buttons;               /* Show close buttons */
    int _group_shrink_threshold;
    int _group_by;
    int _mode;
    gboolean _expand_focused_group;

    gboolean show_mapped_prev;
    gboolean show_iconified_prev;

    guint deferred_desktop_switch_timer;
    int deferred_current_desktop;
    int deferred_active_window_valid;
    Window deferred_active_window;

    int expected_icon_size;

    Atom a_OB_WM_STATE_UNDECORATED;
} TaskbarPlugin;

/******************************************************************************/

static gchar *taskbar_rc = "style 'taskbar-style'\n"
"{\n"
"GtkWidget::focus-padding=0\n" /* FIXME: seem to fix #2821771, not sure if this is ok. */
"GtkWidget::focus-line-width=0\n"
"GtkWidget::focus-padding=0\n"
"GtkButton::default-border={0,0,0,0}\n"
"GtkButton::default-outside-border={0,0,0,0}\n"
"GtkButton::inner-border={0,0,0,0}\n" /* added in gtk+ 2.10 */
"}\n"
"widget '*.taskbar.*' style 'taskbar-style'";

#define DRAG_ACTIVE_DELAY    1000
#define TASK_WIDTH_MAX       200
#define TASK_PADDING         4
#define ALL_WORKSPACES       0xFFFFFFFF		/* 64-bit clean */
#define ICON_ONLY_EXTRA      6		/* Amount needed to have button lay out symmetrically */
#define BUTTON_HEIGHT_EXTRA  4          /* Amount needed to have button not clip icon */

static void set_timer_on_task(Task * tk);
static gboolean task_is_visible_on_current_desktop(Task * tk);
static void recompute_group_visibility_for_class(TaskbarPlugin * tb, TaskClass * tc);
static void recompute_group_visibility_on_current_desktop(TaskbarPlugin * tb);
static void task_draw_label(Task * tk);
static gboolean task_is_visible(Task * tk);
static void task_button_redraw(Task * tk);
static void taskbar_redraw(TaskbarPlugin * tb);
static gboolean accept_net_wm_state(NetWMState * nws);
static gboolean accept_net_wm_window_type(NetWMWindowType * nwwt);
static void task_free_names(Task * tk);
static void task_set_names(Task * tk, Atom source);

static void task_unlink_class(Task * tk);
static TaskClass * taskbar_enter_res_class(TaskbarPlugin * tb, char * res_class, gboolean * name_consumed);
static void task_set_class(Task * tk);

static Task * task_lookup(TaskbarPlugin * tb, Window win);
static void task_delete(TaskbarPlugin * tb, Task * tk, gboolean unlink);
static GdkPixbuf * _wnck_gdk_pixbuf_get_from_pixmap(Pixmap xpixmap, int width, int height);
static GdkPixbuf * apply_mask(GdkPixbuf * pixbuf, GdkPixbuf * mask);
static GdkPixbuf * get_wm_icon(Window task_win, int required_width, int required_height, Atom source, Atom * current_source);
static void task_update_icon(Task * tk, Atom source);
static void task_update_grouping(Task * tk, int group_by);
static gboolean flash_window_timeout(Task * tk);
static void task_set_urgency(Task * tk);
static void task_clear_urgency(Task * tk);
static void task_raise_window(Task * tk, guint32 time);
static void taskbar_popup_set_position(GtkWidget * menu, gint * px, gint * py, gboolean * push_in, gpointer data);
static void task_group_menu_destroy(TaskbarPlugin * tb);
static gboolean taskbar_task_control_event(GtkWidget * widget, GdkEventButton * event, Task * tk, gboolean popup_menu);
static gboolean taskbar_button_press_event(GtkWidget * widget, GdkEventButton * event, Task * tk);
static gboolean taskbar_popup_activate_event(GtkWidget * widget, GdkEventButton * event, Task * tk);
static gboolean taskbar_button_drag_motion_timeout(Task * tk);
static gboolean taskbar_button_drag_motion(GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y, guint time, Task * tk);
static void taskbar_button_drag_leave(GtkWidget * widget, GdkDragContext * drag_context, guint time, Task * tk);
static void taskbar_button_enter(GtkWidget * widget, Task * tk);
static void taskbar_button_leave(GtkWidget * widget, Task * tk);
static gboolean taskbar_button_scroll_event(GtkWidget * widget, GdkEventScroll * event, Task * tk);
static void taskbar_button_size_allocate(GtkWidget * btn, GtkAllocation * alloc, Task * tk);
static void taskbar_image_size_allocate(GtkWidget * img, GtkAllocation * alloc, Task * tk);
static void taskbar_update_style(TaskbarPlugin * tb);
static void task_update_style(Task * tk, TaskbarPlugin * tb);
static void task_build_gui_label(TaskbarPlugin * tb, Task* tk);
static void task_build_gui_button_close(TaskbarPlugin * tb, Task* tk);
static void task_build_gui(TaskbarPlugin * tb, Task * tk);
static void taskbar_net_client_list(GtkWidget * widget, TaskbarPlugin * tb);
static void taskbar_net_current_desktop(GtkWidget * widget, TaskbarPlugin * tb);
static void taskbar_net_number_of_desktops(GtkWidget * widget, TaskbarPlugin * tb);
static void taskbar_net_desktop_names(FbEv * fbev, TaskbarPlugin * tb);
static void taskbar_net_active_window(GtkWidget * widget, TaskbarPlugin * tb);
static gboolean task_has_urgency(Task * tk);
static void taskbar_property_notify_event(TaskbarPlugin * tb, XEvent *ev);
static GdkFilterReturn taskbar_event_filter(XEvent * xev, GdkEvent * event, TaskbarPlugin * tb);

static void menu_raise_window(GtkWidget * widget, TaskbarPlugin * tb);
static void menu_restore_window(GtkWidget * widget, TaskbarPlugin * tb);
static void menu_maximize_window(GtkWidget * widget, TaskbarPlugin * tb);
static void menu_iconify_window(GtkWidget * widget, TaskbarPlugin * tb);
static void menu_ungroup_window(GtkWidget * widget, TaskbarPlugin * tb);
static void menu_move_to_workspace(GtkWidget * widget, TaskbarPlugin * tb);
static void menu_close_window(GtkWidget * widget, TaskbarPlugin * tb);

static void task_adjust_menu(Task * tk, gboolean from_popup_menu);
static void taskbar_make_menu(TaskbarPlugin * tb);

static void taskbar_window_manager_changed(GdkScreen * screen, TaskbarPlugin * tb);

static void taskbar_build_gui(Plugin * p);

static int taskbar_constructor(Plugin * p, char ** fp);
static void taskbar_destructor(Plugin * p);
static void taskbar_apply_configuration(Plugin * p);
static void taskbar_configure(Plugin * p, GtkWindow * parent);
static void taskbar_save_configuration(Plugin * p, FILE * fp);
static void taskbar_panel_configuration_changed(Plugin * p);

/******************************************************************************/

/* Set an urgency timer on a task. */
static void set_timer_on_task(Task * tk)
{
    gint interval;
    g_object_get(gtk_widget_get_settings(tk->button), "gtk-cursor-blink-time", &interval, NULL);
    tk->flash_timeout = g_timeout_add(interval, (GSourceFunc) flash_window_timeout, tk);
}

static int get_task_button_max_width(TaskbarPlugin * tb)
{
    int icon_mode_max_width = tb->icon_size + ICON_ONLY_EXTRA + (tb->_show_close_buttons ? tb->extra_size : 0);
    if (tb->show_titles && tb->task_width_max > icon_mode_max_width) {
        return tb->task_width_max;
    } else {
        return icon_mode_max_width;
    }
}

static gboolean get_task_button_expandable(TaskbarPlugin * tb) {
        return tb->single_window || tb->task_width_max < 1;
}

static int task_button_is_really_flat(TaskbarPlugin * tb)
{
    return ( tb->single_window || tb->flat_button );
}

static char* task_get_displayed_name(Task * tk)
{
    if (tk->iconified) {
        if (!tk->name_iconified) {
            tk->name_iconified = g_strdup_printf("[%s]", tk->name);
        }
        return tk->name_iconified;
    } else {
        return tk->name;
    }
}

static gchar* taskbar_get_desktop_name(TaskbarPlugin * tb, int desktop, const char* defval)
{
    gchar * name = NULL;
    if (desktop == ALL_WORKSPACES)
        name = g_strdup(_("_All workspaces"));
    else if (desktop < tb->number_of_desktop_names && tb->desktop_names)
        name = g_strdup(tb->desktop_names[desktop]);
    else if (defval)
        name = g_strdup(defval);
    else
        name = g_strdup_printf("%d", desktop + 1);
    return name;
}

static gchar* task_get_desktop_name(Task * tk, const char* defval)
{
    return taskbar_get_desktop_name(tk->tb, tk->desktop, defval);
}

static int task_class_is_grouped(TaskbarPlugin * tb, TaskClass * tc)
{
    if (!tb->grouped_tasks)
        return FALSE;

    if (tc && tc->manual_expand_state)
        return !tc->expand;

    if (tb->_expand_focused_group && tb->focused && tb->focused->res_class == tc)
        return FALSE;

    int visible_count = tc ? tc->visible_count : 1;
    return (tb->_group_shrink_threshold > 0) && (visible_count >= tb->_group_shrink_threshold);
}

static gboolean task_has_visible_close_button(Task * tk)
{
    return tk->tb->_show_close_buttons && !task_class_is_grouped(tk->tb, tk->res_class);
}

/* Determine if a task is visible considering only its desktop placement. */
static gboolean task_is_visible_on_desktop(Task * tk, int desktop)
{
    return ( (tk->desktop == ALL_WORKSPACES) || (tk->desktop == desktop) || (tk->tb->show_all_desks) );
}

/* Determine if a task is visible considering only its desktop placement. */
static gboolean task_is_visible_on_current_desktop(Task * tk)
{
    return task_is_visible_on_desktop(tk, tk->tb->current_desktop);
}

static gboolean taskbar_has_visible_tasks_on_desktop(TaskbarPlugin * tb, int desktop)
{
    Task * tk;
    for (tk = tb->task_list; tk != NULL; tk = tk->task_flink)
        if (task_is_visible_on_desktop(tk,  desktop))
            return TRUE;
    return FALSE;
}

/* Recompute the visible task for a class when the class membership changes.
 * Also transfer the urgency state to the visible task if necessary. */
static void recompute_group_visibility_for_class(TaskbarPlugin * tb, TaskClass * tc)
{
    if (!tc)
        return;

    Task * prev_visible_task = tc->visible_task;

    tc->visible_count = 0;
    tc->visible_task = NULL;
    tc->visible_name = NULL;
    Task * flashing_task = NULL;
    gboolean class_has_urgency = FALSE;
    Task * tk;

    Task * visible_task_candidate = NULL;
    int visible_task_prio = 0;

    for (tk = tc->res_class_head; tk != NULL; tk = tk->res_class_flink)
    {
        if (task_is_visible_on_current_desktop(tk))
        {
            tc->visible_count += 1;

            if (!visible_task_candidate || tk->focus_timestamp > visible_task_prio)
            {
                visible_task_prio = tk->focus_timestamp;
                visible_task_candidate = tk;
            }

            /* Compute summary bit for urgency anywhere in the class. */
            if (tk->urgency)
                class_has_urgency = TRUE;

            /* If there is urgency, record the currently flashing task. */
            if (tk->flash_timeout != 0)
                flashing_task = tk;

            /* Compute the visible name.  If all visible windows have the same title, use that.
             * Otherwise, use the class name.  This follows WNCK.
             * Note that the visible name is not a separate string, but is set to point to one of the others. */
            if (tc->visible_name == NULL)
                tc->visible_name = tk->name;
            else if ((tc->visible_name != tc->res_class)
            && (tc->visible_name != NULL) && (tk->name != NULL)
            && (strcmp(tc->visible_name, tk->name) != 0))
                tc->visible_name = tc->res_class;
        }
    }

    tc->visible_task = visible_task_candidate;

    /* Transfer the flash timeout to the visible task. */
    if (class_has_urgency)
    {
        if (flashing_task == NULL)
        {
            /* Set the flashing context and flash the window immediately. */
            tc->visible_task->flash_state = TRUE;
            flash_window_timeout(tc->visible_task);

            /* Set the timer, since none is set. */
            set_timer_on_task(tc->visible_task);
        }
        else if (flashing_task != tc->visible_task)
        {
            /* Reset the timer on the new representative.
             * There will be a slight hiccup on the flash cadence. */
            g_source_remove(flashing_task->flash_timeout);
            flashing_task->flash_timeout = 0;
            tc->visible_task->flash_state = flashing_task->flash_state;
            flashing_task->flash_state = FALSE;
            set_timer_on_task(tc->visible_task);
        }   
    }
    else
    {
        /* No task has urgency.  Cancel the timer if one is set. */
        if (flashing_task != NULL)
        {
            g_source_remove(flashing_task->flash_timeout);
            flashing_task->flash_state = FALSE;
            flashing_task->flash_timeout = 0;
        }
    }

    if (tc->visible_task)
        task_update_style(tc->visible_task, tb);

    if (prev_visible_task && prev_visible_task != tc->visible_task)
        task_button_redraw(prev_visible_task);
}

/* Recompute the visible task for all classes when the desktop changes. */
static void recompute_group_visibility_on_current_desktop(TaskbarPlugin * tb)
{
    TaskClass * tc;
    for (tc = tb->res_class_list; tc != NULL; tc = tc->res_class_flink)
    {
        recompute_group_visibility_for_class(tb, tc);
    }
}

/* Draw the label and tooltip on a taskbar button. */
static void task_draw_label(Task * tk)
{
    TaskClass * tc = tk->res_class;
    gboolean bold_style = (((tk->entered_state) || (tk->flash_state)) && (tk->tb->flat_button));
    if (task_class_is_grouped(tk->tb, tc) && (tc) && (tc->visible_task == tk))
    {
        char * label = g_strdup_printf("(%d) %s", tc->visible_count, tc->visible_name);
        gtk_widget_set_tooltip_text(tk->button, label);
        if (tk->label)
            panel_draw_label_text(tk->tb->plug->panel, tk->label, label, bold_style, task_button_is_really_flat(tk->tb));
        g_free(label);
    }
    else
    {
        char * name = task_get_displayed_name(tk);
        if (tk->tb->tooltips)
            gtk_widget_set_tooltip_text(tk->button, name);
        if (tk->label)
            panel_draw_label_text(tk->tb->plug->panel, tk->label, name, bold_style, task_button_is_really_flat(tk->tb));
    }
}

/* Determine if a task is visible. */
static gboolean task_is_visible(Task * tk)
{
    TaskbarPlugin * tb = tk->tb;

    /* Not visible due to grouping. */
    if (task_class_is_grouped(tb, tk->res_class) && (tk->res_class) && (tk->res_class->visible_task != tk))
        return FALSE;

    /* In single_window mode only focused task is visible. */
    if (tb->single_window && !tk->focused)
        return FALSE;

    /* Hide iconified or mapped tasks? */
    if (!tb->single_window && !((tk->iconified && tb->show_iconified) || (!tk->iconified && tb->show_mapped)) )
        return FALSE;

    /* Desktop placement. */
    return task_is_visible_on_current_desktop(tk);
}

static void task_button_redraw_button_state(Task * tk, TaskbarPlugin * tb)
{
    if( task_button_is_really_flat(tb) )
    {
        gtk_toggle_button_set_active((GtkToggleButton*)tk->button, FALSE);
        gtk_button_set_relief(GTK_BUTTON(tk->button), GTK_RELIEF_NONE);
    }
    else
    {
        gtk_toggle_button_set_active((GtkToggleButton*)tk->button, tk->focused);
        gtk_button_set_relief(GTK_BUTTON(tk->button), GTK_RELIEF_NORMAL);
    }
}

static gboolean task_adapt_to_allocated_size(Task * tk)
{
    tk->adapt_to_allocated_size_idle_cb = 0;

    gboolean button_close_visible = FALSE;
    if (task_has_visible_close_button(tk)) {
        int task_button_required_width = tk->tb->icon_size + ICON_ONLY_EXTRA + tk->tb->extra_size;
        button_close_visible = tk->button_alloc.width >= task_button_required_width;
    }
    if (tk->button_close)
        gtk_widget_set_visible(tk->button_close, button_close_visible);

    return FALSE;
}

/* Redraw a task button. */
static void task_button_redraw(Task * tk)
{
    TaskbarPlugin * tb = tk->tb;

    if (task_is_visible(tk))
    {
        task_button_redraw_button_state(tk, tb);
        task_draw_label(tk);
        icon_grid_set_visible(tb->icon_grid, tk->button, TRUE);
    }
    else
    {
        icon_grid_set_visible(tb->icon_grid, tk->button, FALSE);
    }
}

/* Redraw all tasks in the taskbar. */
static void taskbar_redraw(TaskbarPlugin * tb)
{
    if (!tb->icon_grid)
        return;

    icon_grid_defer_updates(tb->icon_grid);
    Task * tk;
    for (tk = tb->task_list; tk != NULL; tk = tk->task_flink)
        task_button_redraw(tk);
    icon_grid_resume_updates(tb->icon_grid);
}

/* Determine if a task should be visible given its NET_WM_STATE. */
static gboolean accept_net_wm_state(NetWMState * nws)
{
    return ( ! (nws->skip_taskbar));
}

/* Determine if a task should be visible given its NET_WM_WINDOW_TYPE. */
static gboolean accept_net_wm_window_type(NetWMWindowType * nwwt)
{
    return ( ! ((nwwt->desktop) || (nwwt->dock) || (nwwt->splash)));
}

/* Free the names associated with a task. */
static void task_free_names(Task * tk)
{
    g_free(tk->name);
    g_free(tk->name_iconified);
    tk->name = tk->name_iconified = NULL;
}

/* Set the names associated with a task.
 * This is expected to be the same as the title the window manager is displaying. */
static void task_set_names(Task * tk, Atom source)
{
    char * name = NULL;

    /* Try _NET_WM_VISIBLE_NAME, which supports UTF-8.
     * If it is set, the window manager is displaying it as the window title. */
    if ((source == None) || (source == a_NET_WM_VISIBLE_NAME))
    {
        name = get_utf8_property(tk->win,  a_NET_WM_VISIBLE_NAME);
        if (name != NULL)
            tk->name_source = a_NET_WM_VISIBLE_NAME;
    }

    /* Try _NET_WM_NAME, which supports UTF-8, but do not overwrite _NET_WM_VISIBLE_NAME. */
    if ((name == NULL)
    && ((source == None) || (source == a_NET_WM_NAME))
    && ((tk->name_source == None) || (tk->name_source == a_NET_WM_NAME) || (tk->name_source == XA_WM_NAME)))
    {
        name = get_utf8_property(tk->win,  a_NET_WM_NAME);
        if (name != NULL)
            tk->name_source = a_NET_WM_NAME;
    }

    /* Try WM_NAME, which supports only ISO-8859-1, but do not overwrite _NET_WM_VISIBLE_NAME or _NET_WM_NAME. */
    if ((name == NULL)
    && ((source == None) || (source == XA_WM_NAME))
    && ((tk->name_source == None) || (tk->name_source == XA_WM_NAME)))
    {
        name = get_textproperty(tk->win,  XA_WM_NAME);
        if (name != NULL)
            tk->name_source = XA_WM_NAME;
    }

    /* Set the name into the task context, and also on the tooltip. */
    if (name != NULL)
    {
        task_free_names(tk);
        tk->name = g_strdup(name);
        g_free(name);

        /* Update tk->res_class->visible_name as it may point to freed tk->name. */
        if (tk->res_class && tk->tb)
        {
            recompute_group_visibility_for_class(tk->tb, tk->res_class);
        }

        /* Redraw the button. */
        task_button_redraw(tk);
    }
}

/******************************************************************************/

/* Task Class managment functions. */

/* Unlink a task from the class list because its class changed or it was deleted. */
static void task_unlink_class(Task * tk)
{
    ENTER;
    TaskClass * tc = tk->res_class;
    if (tc != NULL)
    {
        tk->res_class = NULL;

        if (tc->visible_task == tk)
            tc->visible_task = NULL;

        /* Remove from per-class task list. */
        if (tc->res_class_head == tk)
        {
            /* Removing the head of the list.  This causes a new task to be the visible task, so we redraw. */
            tc->res_class_head = tk->res_class_flink;
            tk->res_class_flink = NULL;
            if (tc->res_class_head != NULL)
                task_button_redraw(tc->res_class_head);
        }
        else
        {
            /* Locate the task and its predecessor in the list and then remove it.  For safety, ensure it is found. */
            Task * tk_pred = NULL;
            Task * tk_cursor;
            for (
              tk_cursor = tc->res_class_head;
              ((tk_cursor != NULL) && (tk_cursor != tk));
              tk_pred = tk_cursor, tk_cursor = tk_cursor->res_class_flink) ;
            if (tk_cursor == tk)
                tk_pred->res_class_flink = tk->res_class_flink;
            tk->res_class_flink = NULL;
        }

        /* Recompute group visibility. */
        recompute_group_visibility_for_class(tk->tb, tc);
    }
    RET();
}

/* Enter class with specified name. */
static TaskClass * taskbar_enter_res_class(TaskbarPlugin * tb, char * res_class, gboolean * name_consumed)
{
    ENTER;
    /* Find existing entry or insertion point. */
    *name_consumed = FALSE;
    TaskClass * tc_pred = NULL;
    TaskClass * tc;
    for (tc = tb->res_class_list; tc != NULL; tc_pred = tc, tc = tc->res_class_flink)
    {
        int status = strcmp(res_class, tc->res_class);
        if (status == 0)
            RET(tc);
        if (status < 0)
            break;
    }

    /* Insert new entry. */
    tc = g_new0(TaskClass, 1);
    tc->res_class = res_class;
    *name_consumed = TRUE;
    if (tc_pred == NULL)
    {
        tc->res_class_flink = tb->res_class_list;
        tb->res_class_list = tc;
    }
    else
    {
        tc->res_class_flink = tc_pred->res_class_flink;
	tc_pred->res_class_flink = tc;
    }
    RET(tc);
}

static gchar* task_get_res_class(Task * tk) {
    ENTER;
    /* Read the WM_CLASS property. */
    XClassHint ch;
    ch.res_name = NULL;
    ch.res_class = NULL;
    XGetClassHint(GDK_DISPLAY(), tk->win, &ch);

    gchar * res_class = NULL;
    if (ch.res_class != NULL)
        res_class = g_locale_to_utf8(ch.res_class, -1, NULL, NULL, NULL);

    if (ch.res_name != NULL && (!res_class || !strlen(res_class)))
        res_class = g_locale_to_utf8(ch.res_name, -1, NULL, NULL, NULL);

    if (!res_class)
        res_class = g_strdup("");

    if (ch.res_class != NULL)
        XFree(ch.res_class);

    if (ch.res_name != NULL)
        XFree(ch.res_name);

    RET(res_class);
}

/* Set the class associated with a task. */
static void task_set_class(Task * tk)
{
    ENTER;

    g_assert(tk != NULL);

    gchar * res_class = NULL;
    
    if (tk->override_class_name != (char*) -1) {
         res_class = g_strdup(tk->override_class_name);
    } else {
        switch (tk->tb->_group_by) {
            case GROUP_BY_CLASS:
                res_class = task_get_res_class(tk); break;
            case GROUP_BY_WORKSPACE:
                res_class = task_get_desktop_name(tk, NULL); break;
            case GROUP_BY_STATE:
                res_class = g_strdup(
                    (tk->urgency) ? _("Urgency") : 
                    (tk->iconified) ? _("Iconified") : 
                    _("Mapped")
                );
                break;
        }
    }

    if (res_class != NULL)
    {
        DBG("Task %s has res_class %s\n", tk->name, res_class);

        gboolean name_consumed;
        TaskClass * tc = taskbar_enter_res_class(tk->tb, res_class, &name_consumed);
        if ( ! name_consumed) g_free(res_class);

        /* If the task changed class, update data structures. */
        TaskClass * old_tc = tk->res_class;
        if (old_tc != tc)
        {
            /* Unlink from previous class, if any. */
            if (old_tc)
                task_unlink_class(tk);

            if (!tc->timestamp)
                tc->timestamp = tk->timestamp;

            /* Add to end of per-class task list.  Do this to keep the popup menu in order of creation. */
            if (tc->res_class_head == NULL)
                tc->res_class_head = tk;
            else
            {
                Task * tk_pred;
                for (tk_pred = tc->res_class_head; tk_pred->res_class_flink != NULL; tk_pred = tk_pred->res_class_flink) ;
                tk_pred->res_class_flink = tk;
                g_assert(tk->res_class_flink == NULL);
                task_button_redraw(tk);
            }
            tk->res_class = tc;

            /* Recompute group visibility. */
            recompute_group_visibility_for_class(tk->tb, tc);
        }
    }

    RET();
}

static void task_set_override_class(Task * tk, char * class_name)
{
     task_unlink_class(tk);

     if (tk->override_class_name != (char*) -1 && tk->override_class_name)
         g_free(tk->override_class_name);

     if (class_name != (char*) -1 && class_name)
         tk->override_class_name = g_strdup(class_name);
     else
         tk->override_class_name = class_name;

     task_update_grouping(tk, -1);
}

/******************************************************************************/

/* Look up a task in the task list. */
static Task * task_lookup(TaskbarPlugin * tb, Window win)
{
    Task * tk;
    for (tk = tb->task_list; tk != NULL; tk = tk->task_flink)
    {
        if (tk->win == win)
	    return tk;
    }
    return NULL;
}

/* Delete a task and optionally unlink it from the task list. */
static void task_delete(TaskbarPlugin * tb, Task * tk, gboolean unlink)
{
    ENTER;

    DBG("Deleting task %s (0x%x)\n", tk->name, (int)tk);

    /* If we think this task had focus, remove that. */
    if (tb->focused == tk)
        tb->focused = NULL;

    /* If there are deferred calls, remove them. */
    if (tk->adapt_to_allocated_size_idle_cb != 0)
        g_source_remove(tk->adapt_to_allocated_size_idle_cb);
    if (tk->update_icon_idle_cb != 0)
        g_source_remove(tk->update_icon_idle_cb);

    if (tk->override_class_name != (char*) -1 && tk->override_class_name)
         g_free(tk->override_class_name);

    /* If there is an urgency timeout, remove it. */
    if (tk->flash_timeout != 0)
        g_source_remove(tk->flash_timeout);

    /* Deallocate structures. */
    icon_grid_remove(tb->icon_grid, tk->button);
    task_free_names(tk);
    task_unlink_class(tk);

    /* If requested, unlink the task from the task list.
     * If not requested, the caller will do this. */
    if (unlink)
    {
        if (tb->task_list == tk)
            tb->task_list = tk->task_flink;
        else
        {
            /* Locate the task and its predecessor in the list and then remove it.  For safety, ensure it is found. */
            Task * tk_pred = NULL;
            Task * tk_cursor;
            for (
              tk_cursor = tb->task_list;
              ((tk_cursor != NULL) && (tk_cursor != tk));
              tk_pred = tk_cursor, tk_cursor = tk_cursor->task_flink) ;
            if (tk_cursor == tk)
                tk_pred->task_flink = tk->task_flink;
        }
    }

    /* Deallocate the task structure. */
    g_free(tk);

    RET();
}

/* Get a pixbuf from a pixmap.
 * Originally from libwnck, Copyright (C) 2001 Havoc Pennington. */
static GdkPixbuf * _wnck_gdk_pixbuf_get_from_pixmap(Pixmap xpixmap, int width, int height)
{
    /* Get the drawable. */
    GdkDrawable * drawable = gdk_xid_table_lookup(xpixmap);
    if (drawable != NULL)
        g_object_ref(G_OBJECT(drawable));
    else
        drawable = gdk_pixmap_foreign_new(xpixmap);

    GdkColormap * colormap = NULL;
    GdkPixbuf * retval = NULL;
    if (drawable != NULL)
    {
        /* Get the colormap.
         * If the drawable has no colormap, use no colormap or the system colormap as recommended in the documentation of gdk_drawable_get_colormap. */
        colormap = gdk_drawable_get_colormap(drawable);
        gint depth = gdk_drawable_get_depth(drawable);
        if (colormap != NULL)
            g_object_ref(G_OBJECT(colormap));
        else if (depth == 1)
            colormap = NULL;
        else
        {
            colormap = gdk_screen_get_system_colormap(gdk_drawable_get_screen(drawable));
            g_object_ref(G_OBJECT(colormap));
        }

        /* Be sure we aren't going to fail due to visual mismatch. */
        if ((colormap != NULL) && (gdk_colormap_get_visual(colormap)->depth != depth))
        {
            g_object_unref(G_OBJECT(colormap));
            colormap = NULL;
        }

        /* Do the major work. */
        retval = gdk_pixbuf_get_from_drawable(NULL, drawable, colormap, 0, 0, 0, 0, width, height);
    }

    /* Clean up and return. */
    if (colormap != NULL)
        g_object_unref(G_OBJECT(colormap));
    if (drawable != NULL)
        g_object_unref(G_OBJECT(drawable));
    return retval;
}

/* Apply a mask to a pixbuf.
 * Originally from libwnck, Copyright (C) 2001 Havoc Pennington. */
static GdkPixbuf * apply_mask(GdkPixbuf * pixbuf, GdkPixbuf * mask)
{
    /* Initialize. */
    int w = MIN(gdk_pixbuf_get_width(mask), gdk_pixbuf_get_width(pixbuf));
    int h = MIN(gdk_pixbuf_get_height(mask), gdk_pixbuf_get_height(pixbuf));
    GdkPixbuf * with_alpha = gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
    guchar * dst = gdk_pixbuf_get_pixels(with_alpha);
    guchar * src = gdk_pixbuf_get_pixels(mask);
    int dst_stride = gdk_pixbuf_get_rowstride(with_alpha);
    int src_stride = gdk_pixbuf_get_rowstride(mask);

    /* Loop to do the work. */
    int i;
    for (i = 0; i < h; i += 1)
    {
        int j;
        for (j = 0; j < w; j += 1)
        {
            guchar * s = src + i * src_stride + j * 3;
            guchar * d = dst + i * dst_stride + j * 4;

            /* s[0] == s[1] == s[2], they are 255 if the bit was set, 0 otherwise. */
            d[3] = ((s[0] == 0) ? 0 : 255);	/* 0 = transparent, 255 = opaque */
        }
    }

    return with_alpha;
}

/* Get an icon from the window manager for a task, and scale it to a specified size. */
static GdkPixbuf * get_wm_icon(Window task_win, int required_width, int required_height, Atom source, Atom * current_source)
{
    /* The result. */
    GdkPixbuf * pixmap = NULL;
    Atom possible_source = None;
    int result = -1;

    if ((source == None) || (source == a_NET_WM_ICON))
    {
        /* Important Notes:
         * According to freedesktop.org document:
         * http://standards.freedesktop.org/wm-spec/wm-spec-1.4.html#id2552223
         * _NET_WM_ICON contains an array of 32-bit packed CARDINAL ARGB.
         * However, this is incorrect. Actually it's an array of long integers.
         * Toolkits like gtk+ use unsigned long here to store icons.
         * Besides, according to manpage of XGetWindowProperty, when returned format,
         * is 32, the property data will be stored as an array of longs
         * (which in a 64-bit application will be 64-bit values that are
         * padded in the upper 4 bytes).
         */

        /* Get the window property _NET_WM_ICON, if possible. */
        Atom type = None;
        int format;
        gulong nitems;
        gulong bytes_after;
        gulong * data = NULL;
        result = XGetWindowProperty(
            GDK_DISPLAY(),
            task_win,
            a_NET_WM_ICON,
            0, G_MAXLONG,
            False, XA_CARDINAL,
            &type, &format, &nitems, &bytes_after, (void *) &data);

        /* Inspect the result to see if it is usable.  If not, and we got data, free it. */
        if ((type != XA_CARDINAL) || (nitems <= 0))
        {
            if (data != NULL)
                XFree(data);
            result = -1;
        }

        /* If the result is usable, extract the icon from it. */
        if (result == Success)
        {
            /* Get the largest icon available, unless there is one that is the desired size. */
            /* FIXME: should we try to find an icon whose size is closest to
             * required_width and required_height to reduce unnecessary resizing? */
            gulong * pdata = data;
            gulong * pdata_end = data + nitems;
            gulong * max_icon = NULL;
            gulong max_w = 0;
            gulong max_h = 0;
            while ((pdata + 2) < pdata_end)
            {
                /* Extract the width and height. */
                gulong w = pdata[0];
                gulong h = pdata[1];
                gulong size = w * h;
                pdata += 2;

                /* Bounds check the icon. */
                if (pdata + size > pdata_end)
                    break;

                /* Rare special case: the desired size is the same as icon size. */
                if ((required_width == w) && (required_height == h))
                {
                    max_icon = pdata;
                    max_w = w;
                    max_h = h;
                    break;
                }

                /* If the icon is the largest so far, capture it. */
                if ((w > max_w) && (h > max_h))
                {
                    max_icon = pdata;
                    max_w = w;
                    max_h = h;
                }
                pdata += size;
            }

            /* If an icon was extracted, convert it to a pixbuf.
             * Its size is max_w and max_h. */
            if (max_icon != NULL)
            {
                /* Allocate enough space for the pixel data. */
                gulong len = max_w * max_h;
                guchar * pixdata = g_new(guchar, len * 4);

                /* Loop to convert the pixel data. */
                guchar * p = pixdata;
                int i;
                for (i = 0; i < len; p += 4, i += 1)
                {
                    guint argb = max_icon[i];
                    guint rgba = (argb << 8) | (argb >> 24);
                    p[0] = rgba >> 24;
                    p[1] = (rgba >> 16) & 0xff;
                    p[2] = (rgba >> 8) & 0xff;
                    p[3] = rgba & 0xff;
                }
            
                /* Initialize a pixmap with the pixel data. */
                pixmap = gdk_pixbuf_new_from_data(
                    pixdata,
                    GDK_COLORSPACE_RGB,
                    TRUE, 8,	/* has_alpha, bits_per_sample */
                    max_w, max_h, max_w * 4,
                    (GdkPixbufDestroyNotify) g_free,
                    NULL);
                possible_source = a_NET_WM_ICON;
            }
	    else
	        result = -1;

            /* Free the X property data. */
            XFree(data);
        }
    }

    /* No icon available from _NET_WM_ICON.  Next try WM_HINTS, but do not overwrite _NET_WM_ICON. */
    if ((result != Success) && (*current_source != a_NET_WM_ICON)
    && ((source == None) || (source != a_NET_WM_ICON)))
    {
        XWMHints * hints = XGetWMHints(GDK_DISPLAY(), task_win);
        result = (hints != NULL) ? Success : -1;
        Pixmap xpixmap = None;
        Pixmap xmask = None;

        if (result == Success)
        {
            /* WM_HINTS is available.  Extract the X pixmap and mask. */
            if ((hints->flags & IconPixmapHint))
                xpixmap = hints->icon_pixmap;
            if ((hints->flags & IconMaskHint))
                xmask = hints->icon_mask;
            XFree(hints);
            if (xpixmap != None)
            {
                result = Success;
                possible_source = XA_WM_HINTS;
            }
            else
                result = -1;
        }

        if (result != Success)
        {
            /* No icon available from _NET_WM_ICON or WM_HINTS.  Next try KWM_WIN_ICON. */
            Atom type = None;
            int format;
            gulong nitems;
            gulong bytes_after;
            Pixmap *icons = NULL;
            Atom kwin_win_icon_atom = gdk_x11_get_xatom_by_name("KWM_WIN_ICON");
            result = XGetWindowProperty(
                GDK_DISPLAY(),
                task_win,
                kwin_win_icon_atom,
                0, G_MAXLONG,
                False, kwin_win_icon_atom,
                &type, &format, &nitems, &bytes_after, (void *) &icons);

            /* Inspect the result to see if it is usable.  If not, and we got data, free it. */
            if (type != kwin_win_icon_atom)
            {
                if (icons != NULL)
                    XFree(icons);
                result = -1;
            }

            /* If the result is usable, extract the X pixmap and mask from it. */
            if (result == Success)
            {
                xpixmap = icons[0];
                xmask = icons[1];
                if (xpixmap != None)
                {
                    result = Success;
                    possible_source = kwin_win_icon_atom;
                }
                else
                    result = -1;
            }
        }

        /* If we have an X pixmap, get its geometry.*/
        unsigned int w, h;
        if (result == Success)
        {
            Window unused_win;
            int unused;
            unsigned int unused_2;
            result = XGetGeometry(
                GDK_DISPLAY(), xpixmap,
                &unused_win, &unused, &unused, &w, &h, &unused_2, &unused_2) ? Success : -1;
        }

        /* If we have an X pixmap and its geometry, convert it to a GDK pixmap. */
        if (result == Success) 
        {
            pixmap = _wnck_gdk_pixbuf_get_from_pixmap(xpixmap, w, h);
            result = ((pixmap != NULL) ? Success : -1);
        }

        /* If we have success, see if the result needs to be masked.
         * Failures here are implemented as nonfatal. */
        if ((result == Success) && (xmask != None))
        {
            Window unused_win;
            int unused;
            unsigned int unused_2;
            if (XGetGeometry(
                GDK_DISPLAY(), xmask,
                &unused_win, &unused, &unused, &w, &h, &unused_2, &unused_2))
            {
                /* Convert the X mask to a GDK pixmap. */
                GdkPixbuf * mask = _wnck_gdk_pixbuf_get_from_pixmap(xmask, w, h);
                if (mask != NULL)
                {
                    /* Apply the mask. */
                    GdkPixbuf * masked_pixmap = apply_mask(pixmap, mask);
                    g_object_unref(G_OBJECT(pixmap));
                    g_object_unref(G_OBJECT(mask));
                    pixmap = masked_pixmap;
                }
            }
        }
    }

    /* If we got a pixmap, scale it and return it. */
    if (pixmap == NULL)
        return NULL;
    else
    {
        gulong w = gdk_pixbuf_get_width (pixmap);
        gulong h = gdk_pixbuf_get_height (pixmap);
        if ((w > required_width) || (h > required_height))
        {
            w = required_width;
            h = required_height;
        }

        GdkPixbuf * ret = gdk_pixbuf_scale_simple(pixmap, w, h, GDK_INTERP_TILES);
        g_object_unref(pixmap);
        *current_source = possible_source;
        return ret;
    }
}

/* Update the icon of a task. */
static GdkPixbuf * task_create_icon(Task * tk, Atom source, int icon_size)
{
    TaskbarPlugin * tb = tk->tb;

    /* Get the icon from the window's hints. */
    GdkPixbuf * pixbuf = get_wm_icon(tk->win, icon_size, icon_size, source, &tk->image_source);

    /* If that fails, and we have no other icon yet, return the fallback icon. */
    if ((pixbuf == NULL)
    && ((source == None) || (tk->image_source == None)))
    {
        /* Establish the fallback task icon.  This is used when no other icon is available. */
        if (tb->fallback_pixbuf == NULL)
            tb->fallback_pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) icon_xpm);
        g_object_ref(tb->fallback_pixbuf);
        pixbuf = tb->fallback_pixbuf;
    }

    /* Return what we have.  This may be NULL to indicate that no change should be made to the icon. */
    return pixbuf;
}

static void task_update_icon(Task * tk, Atom source)
{
    int icon_size = tk->tb->expected_icon_size > 0 ? tk->tb->expected_icon_size : tk->tb->icon_size;
    if (tk->allocated_icon_size > 0 && tk->allocated_icon_size < icon_size)
        icon_size = tk->allocated_icon_size;

    GdkPixbuf * pixbuf = task_create_icon(tk, source, icon_size);
    if (pixbuf != NULL)
    {
        gtk_image_set_from_pixbuf(GTK_IMAGE(tk->image), pixbuf);
        g_object_unref(pixbuf);
    }

    tk->icon_size = icon_size;
}

static gboolean task_update_icon_cb(Task * tk)
{
    tk->update_icon_idle_cb = 0;
    task_update_icon(tk, None);
    return FALSE;
}

/* Timer expiration for urgency notification.  Also used to draw the button in setting and clearing urgency. */
static gboolean flash_window_timeout(Task * tk)
{
    /* Set state on the button and redraw. */
    if ( ! task_button_is_really_flat(tk->tb))
        gtk_widget_set_state(tk->button, tk->flash_state ? GTK_STATE_SELECTED : GTK_STATE_NORMAL);
    task_draw_label(tk);

    /* Complement the flashing context. */
    tk->flash_state = ! tk->flash_state;
    return TRUE;
}

/* Set urgency notification. */
static void task_set_urgency(Task * tk)
{
    TaskbarPlugin * tb = tk->tb;
    TaskClass * tc = tk->res_class;
    if (task_class_is_grouped(tb, tc))
        recompute_group_visibility_for_class(tk->tb, tc);
    else
    {
        /* Set the flashing context and flash the window immediately. */
        tk->flash_state = TRUE;
        flash_window_timeout(tk);

        /* Set the timer if none is set. */
        if (tk->flash_timeout == 0)
            set_timer_on_task(tk);
    }
}

/* Clear urgency notification. */
static void task_clear_urgency(Task * tk)
{
    TaskbarPlugin * tb = tk->tb;
    TaskClass * tc = tk->res_class;
    if (task_class_is_grouped(tb, tc))
        recompute_group_visibility_for_class(tk->tb, tc);
    else
    {
        /* Remove the timer if one is set. */
        if (tk->flash_timeout != 0)
        {
            g_source_remove(tk->flash_timeout);
            tk->flash_timeout = 0;
        }

        /* Clear the flashing context and unflash the window immediately. */
        tk->flash_state = FALSE;
        flash_window_timeout(tk);
        tk->flash_state = FALSE;
    }
}

/******************************************************************************/

/* Task actions. */

/* Close task window. */
static void task_close(Task * tk)
{
    Xclimsgwm(tk->win, a_WM_PROTOCOLS, a_WM_DELETE_WINDOW);
}

static void task_iconify(Task * tk)
{
    XIconifyWindow(GDK_DISPLAY(), tk->win, DefaultScreen(GDK_DISPLAY()));
}

static void task_raiseiconify(Task * tk, GdkEventButton * event)
{
    /*
     * If the task is iconified, raise it.
     * If the task is not iconified and has focus, iconify it.
     * If the task is not iconified and does not have focus, raise it. */
    if (tk->iconified)
        task_raise_window(tk, event->time);
    else if ((tk->focused) || (tk == tk->tb->focused_previous))
        task_iconify(tk);
    else
        task_raise_window(tk, event->time);
}

static void task_maximize(Task* tk)
{
    GdkWindow * win = gdk_window_foreign_new(tk->win);
    if (tk->maximized) {
        gdk_window_unmaximize(win);
    } else {
        gdk_window_maximize(win);
    }
    gdk_window_unref(win);
}

static void task_shade(Task * tk)
{
    /* Toggle the shaded state of the window. */
    Xclimsg(tk->win, a_NET_WM_STATE,
                2,		/* a_NET_WM_STATE_TOGGLE */
                a_NET_WM_STATE_SHADED,
                0, 0, 0);
}

static void task_undecorate(Task * tk)
{
    /* Toggle the undecorated state of the window. */
    Xclimsg(tk->win, a_NET_WM_STATE,
                2,		/* a_NET_WM_STATE_TOGGLE */
                tk->tb->a_OB_WM_STATE_UNDECORATED,
                0, 0, 0);
}

static void task_fullscreen(Task * tk)
{
    /* Toggle the fullscreen state of the window. */
    Xclimsg(tk->win, a_NET_WM_STATE,
                2,		/* a_NET_WM_STATE_TOGGLE */
                a_NET_WM_STATE_FULLSCREEN,
                0, 0, 0);
}

static void task_stick(Task * tk)
{
    Xclimsg(tk->win, a_NET_WM_DESKTOP, (tk->desktop == ALL_WORKSPACES) ? tk->tb->current_desktop : ALL_WORKSPACES, 0, 0, 0, 0);
}

static void task_show_menu(Task * tk, GdkEventButton * event, Task* visible_task, gboolean from_popup_menu)
{
    /* Right button.  Bring up the window state popup menu. */
    tk->tb->menutask = tk;
    task_adjust_menu(tk, from_popup_menu);
    gtk_menu_popup(
        GTK_MENU(tk->tb->menu),
        NULL, NULL,
        (GtkMenuPositionFunc) taskbar_popup_set_position, (gpointer) visible_task,
        event->button, event->time);
}

static void task_show_window_list_helper(Task * tk_cursor, GtkWidget * menu, TaskbarPlugin * tb)
{
    if (task_is_visible_on_current_desktop(tk_cursor))
    {
        /* The menu item has the name, or the iconified name, and the icon of the application window. */

        gchar * name = task_get_displayed_name(tk_cursor);
        GtkWidget * mi = NULL;
        if (tk_cursor->desktop != tb->current_desktop && tk_cursor->desktop != ALL_WORKSPACES && tb->_group_by != GROUP_BY_WORKSPACE) {
            gchar* wname = task_get_desktop_name(tk_cursor, NULL);
            name = g_strdup_printf("%s [%s]", name, wname);
            mi = gtk_image_menu_item_new_with_label(name);
            g_free(name);
            g_free(wname);
        } else {
            mi = gtk_image_menu_item_new_with_label(name);
        }

        GtkWidget * im = gtk_image_new_from_pixbuf(gtk_image_get_pixbuf(GTK_IMAGE(tk_cursor->image)));
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), im);

        g_signal_connect(mi, "button_press_event", G_CALLBACK(taskbar_popup_activate_event), (gpointer) tk_cursor);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    }
}

static void task_show_window_list(Task * tk, GdkEventButton * event, gboolean similar)
{
    TaskbarPlugin * tb = tk->tb;
    TaskClass * tc = tk->res_class;

    GtkWidget * menu = gtk_menu_new();
    Task * tk_cursor;

    if (similar && task_class_is_grouped(tb, tc))
    {
        if (tc)
        {
            for (tk_cursor = tc->res_class_head; tk_cursor != NULL; tk_cursor = tk_cursor->res_class_flink)
            {
                task_show_window_list_helper(tk_cursor, menu, tb);
            }
        }
        else
        {
            task_show_window_list_helper(tk, menu, tb);
        }
    }
    else
    {
        for (tk_cursor = tb->task_list; tk_cursor != NULL; tk_cursor = tk_cursor->task_flink)
        {
            if (!similar || tk_cursor->res_class == tc)
                task_show_window_list_helper(tk_cursor, menu, tb);
        }
    }

    /* Show the menu.  Set context so we can find the menu later to dismiss it.
     * Use a position-calculation callback to get the menu nicely positioned with respect to the button. */
    gtk_widget_show_all(menu);
    tb->group_menu = menu;
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc) taskbar_popup_set_position, (gpointer) tk, event->button, event->time);

}

static void task_activate_neighbour(Task * tk, GdkEventButton * event, gboolean next, gboolean in_group)
{
    gboolean before = TRUE;
    Task * candidate_before = NULL;
    Task * candidate_after = NULL;

    Task * tk_cursor;

    if (!tk)
        return;

    if (in_group && tk->res_class)
    {
        if (tk->tb->focused && tk->res_class == tk->tb->focused->res_class)
            tk = tk->tb->focused;
    }

    for (tk_cursor = tk->tb->task_list; tk_cursor != NULL; tk_cursor = tk_cursor->task_flink)
    {
        if (tk_cursor == tk)
        {
            before = FALSE;
            if (!next && candidate_before)
                break;
            continue;
        }
        gboolean ok = task_is_visible_on_current_desktop(tk_cursor)
            && (!in_group || (tk->res_class && tk->res_class == tk_cursor->res_class));
        if (ok)
        {
            if (next)
            {
                if (before)
                {
                    if (!candidate_before)
                        candidate_before = tk_cursor;
                }
                else
                {
                    if (!candidate_after)
                    {
                        candidate_after = tk_cursor;
                        break;
                    }
                }
            }
            else // prev
            {
                if (before)
                {
                    candidate_before = tk_cursor;
                }
                else
                {
                    candidate_after = tk_cursor;
                }
            }
        }
    } // end for

    Task * result = NULL;
    if (next)
    {
        if (candidate_after)
            result = candidate_after;
        else
            result = candidate_before;
    }
    else
    {
        if (candidate_before)
            result = candidate_before;
        else
            result = candidate_after;
    }

    if (result)
        task_raise_window(result, event->time);
    else if (in_group)
    {
        if (!tk->focused)
        {
            task_raise_window(tk, event->time);
        }
    }
}

/* Close task window. */
static void task_action(Task * tk, int action, GdkEventButton * event, Task* visible_task, gboolean from_popup_menu)
{
    switch (action) {
      case ACTION_MENU:
        task_show_menu(tk, event, visible_task, from_popup_menu);
        break;
      case ACTION_CLOSE:
        task_close(tk);
        break;
      case ACTION_RAISEICONIFY:
        task_raiseiconify(tk, event);
        break;
      case ACTION_ICONIFY:
        task_iconify(tk);
        break;
      case ACTION_MAXIMIZE:
        task_maximize(tk);
        break;
      case ACTION_SHADE:
        task_shade(tk);
        break;
      case ACTION_UNDECORATE:
        task_undecorate(tk);
        break;
      case ACTION_FULLSCREEN:
        task_fullscreen(tk);
        break;
      case ACTION_STICK:
        task_stick(tk);
        break;
      case ACTION_SHOW_WINDOW_LIST:
        task_show_window_list(tk, event, FALSE);
        break;
      case ACTION_SHOW_SIMILAR_WINDOW_LIST:
        task_show_window_list(tk, event, TRUE);
        break;
      case ACTION_NEXT_WINDOW:
        task_activate_neighbour(tk->tb->focused, event, TRUE, FALSE);
        break;
      case ACTION_PREV_WINDOW:
        task_activate_neighbour(tk->tb->focused, event, FALSE, FALSE);
        break;
      case ACTION_NEXT_WINDOW_IN_CURRENT_GROUP:
        task_activate_neighbour(tk->tb->focused, event, TRUE, TRUE);
        break;
      case ACTION_PREV_WINDOW_IN_CURRENT_GROUP:
        task_activate_neighbour(tk->tb->focused, event, FALSE, TRUE);
        break;
      case ACTION_NEXT_WINDOW_IN_GROUP:
        task_activate_neighbour(tk, event, TRUE, TRUE);
        break;
      case ACTION_PREV_WINDOW_IN_GROUP:
        task_activate_neighbour(tk, event, FALSE, TRUE);
        break;
    }
}


/* Do the proper steps to raise a window.
 * This means removing it from iconified state and bringing it to the front.
 * We also switch the active desktop and viewport if needed. */
static void task_raise_window(Task * tk, guint32 time)
{
    /* Change desktop if needed. */
    if ((tk->desktop != ALL_WORKSPACES) && (tk->desktop != tk->tb->current_desktop))
        Xclimsg(GDK_ROOT_WINDOW(), a_NET_CURRENT_DESKTOP, tk->desktop, 0, 0, 0, 0);

    /* Evaluate use_net_active if not yet done. */
    if ( ! tk->tb->net_active_checked)
    {
        TaskbarPlugin * tb = tk->tb;
        GdkAtom net_active_atom = gdk_x11_xatom_to_atom(a_NET_ACTIVE_WINDOW);
        tb->use_net_active = gdk_x11_screen_supports_net_wm_hint(gtk_widget_get_screen(tb->plug->pwid), net_active_atom);
        tb->net_active_checked = TRUE;
    }

    /* Raise the window.  We can use NET_ACTIVE_WINDOW if the window manager supports it.
     * Otherwise, do it the old way with XMapRaised and XSetInputFocus. */
    if (tk->tb->use_net_active)
        Xclimsg(tk->win, a_NET_ACTIVE_WINDOW, 2, time, 0, 0, 0);
    else
    {
        GdkWindow * gdkwindow = gdk_xid_table_lookup(tk->win);
        if (gdkwindow != NULL)
            gdk_window_show(gdkwindow);
        else
            XMapRaised(GDK_DISPLAY(), tk->win);

	/* There is a race condition between the X server actually executing the XMapRaised and this code executing XSetInputFocus.
	 * If the window is not viewable, the XSetInputFocus will fail with BadMatch. */
	XWindowAttributes attr;
	XGetWindowAttributes(GDK_DISPLAY(), tk->win, &attr);
	if (attr.map_state == IsViewable)
            XSetInputFocus(GDK_DISPLAY(), tk->win, RevertToNone, time);
    }

    /* Change viewport if needed. */
    XWindowAttributes xwa;
    XGetWindowAttributes(GDK_DISPLAY(), tk->win, &xwa);
    Xclimsg(tk->win, a_NET_DESKTOP_VIEWPORT, xwa.x, xwa.y, 0, 0, 0);
}

/******************************************************************************/

/* Task button input message handlers. */

/* Position-calculation callback for grouped-task and window-management popup menu. */
static void taskbar_popup_set_position(GtkWidget * menu, gint * px, gint * py, gboolean * push_in, gpointer data)
{
    Task * tk = (Task *) data;

    /* Get the allocation of the popup menu. */
    GtkRequisition popup_req;
    gtk_widget_size_request(menu, &popup_req);

    /* Determine the coordinates. */
    plugin_popup_set_position_helper(tk->tb->plug, tk->button, menu, &popup_req, px, py);
    *push_in = TRUE;
}

/* Remove the grouped-task popup menu from the screen. */
static void task_group_menu_destroy(TaskbarPlugin * tb)
{
    if (tb->group_menu != NULL)
    {
        gtk_widget_destroy(tb->group_menu);
        tb->group_menu = NULL;
    }
}

/* Handler for "button-press-event" event from taskbar button,
 * or "activate" event from grouped-task popup menu item. */
static gboolean taskbar_task_control_event(GtkWidget * widget, GdkEventButton * event, Task * tk, gboolean popup_menu)
{
    gboolean event_in_close_button = FALSE;
    if (!popup_menu && task_has_visible_close_button(tk) && tk->button_close && gtk_widget_get_visible(GTK_WIDGET(tk->button_close))) {
        // FIXME: какой нормальный способ узнать, находится ли мышь в пределах виджета?
        gint dest_x, dest_y;
        gtk_widget_translate_coordinates(widget, tk->button_close, event->x, event->y, &dest_x, &dest_y);
        if (dest_x >= 0 && dest_y >= 0 && dest_x < GTK_WIDGET(tk->button_close)->allocation.width && dest_y <= GTK_WIDGET(tk->button_close)->allocation.height) {
            event_in_close_button = TRUE;
        }
    }

    if (event_in_close_button && event->button == 1) {
        if (event->type == GDK_BUTTON_PRESS) {
           tk->click_on = tk->button_close;
           return TRUE;
        } else if (event->type == GDK_BUTTON_RELEASE && tk->click_on == tk->button_close) {
           task_close(tk);
           return TRUE;
        }
    }

   tk->click_on = NULL;
   if (event_in_close_button || event->type == GDK_BUTTON_RELEASE)
       return TRUE;

    TaskbarPlugin * tb = tk->tb;
    TaskClass * tc = tk->res_class;
    if (task_class_is_grouped(tb, tc) && (GTK_IS_BUTTON(widget)))
    {
        /* If this is a grouped-task representative, meaning that there is a class with at least two windows,
         * bring up a popup menu listing all the class members. */
        task_show_window_list(tk, event, TRUE);
    }
    else
    {
        /* Not a grouped-task representative, or entered from the grouped-task popup menu. */

        Task * visible_task = (
            (tb->single_window) ? tb->focused :
            (!task_class_is_grouped(tb, tk->res_class)) ? tk :
            (tk->res_class) ? tk->res_class->visible_task :
            tk);
        task_group_menu_destroy(tb);

        int action = ACTION_NONE;
        switch (event->button) {
            case 1: action = (event->state & GDK_SHIFT_MASK) ? tk->tb->shift_button1_action : tk->tb->button1_action; break;
            case 2: action = (event->state & GDK_SHIFT_MASK) ? tk->tb->shift_button2_action : tk->tb->button2_action; break;
            case 3: action = (event->state & GDK_SHIFT_MASK) ? tk->tb->shift_button3_action : tk->tb->button3_action; break;
        }
        if (popup_menu && (action == ACTION_SHOW_SIMILAR_WINDOW_LIST || action == ACTION_SHOW_WINDOW_LIST))
            action = ACTION_RAISEICONIFY;
        task_action(tk, action, event, visible_task, popup_menu);
    }

    /* As a matter of policy, avoid showing selected or prelight states on flat buttons. */
    if (task_button_is_really_flat(tb))
        gtk_widget_set_state(widget, GTK_STATE_NORMAL);
    return TRUE;
}

/* Handler for "button-press-event" event from taskbar button. */
static gboolean taskbar_button_press_event(GtkWidget * widget, GdkEventButton * event, Task * tk)
{
    if (event->state & GDK_CONTROL_MASK && event->button == 3) {
        Plugin* p = tk->tb->plug;
        lxpanel_show_panel_menu( p->panel, p, event );
        return TRUE;
    }

    return taskbar_task_control_event(widget, event, tk, FALSE);
}

/* Handler for "button-release-event" event from taskbar button. */
static gboolean taskbar_button_release_event(GtkWidget * widget, GdkEventButton * event, Task * tk)
{
    return taskbar_task_control_event(widget, event, tk, FALSE);
}

/* Handler for "activate" event from grouped-task popup menu item. */
static gboolean taskbar_popup_activate_event(GtkWidget * widget, GdkEventButton * event, Task * tk)
{
    return taskbar_task_control_event(widget, event, tk, TRUE);
}

/* Handler for "drag-motion" timeout. */
static gboolean taskbar_button_drag_motion_timeout(Task * tk)
{
    guint time = gtk_get_current_event_time();
    task_raise_window(tk, ((time != 0) ? time : CurrentTime));
    tk->tb->dnd_delay_timer = 0;
    return FALSE;
}

/* Handler for "drag-motion" event from taskbar button. */
static gboolean taskbar_button_drag_motion(GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y, guint time, Task * tk)
{
    /* Prevent excessive motion notification. */
    if (tk->tb->dnd_delay_timer == 0)
        tk->tb->dnd_delay_timer = g_timeout_add(DRAG_ACTIVE_DELAY, (GSourceFunc) taskbar_button_drag_motion_timeout, tk);
    gdk_drag_status(drag_context, 0, time);
    return TRUE;
}

/* Handler for "drag-leave" event from taskbar button. */
static void taskbar_button_drag_leave(GtkWidget * widget, GdkDragContext * drag_context, guint time, Task * tk)
{
    /* Cancel the timer if set. */
    if (tk->tb->dnd_delay_timer != 0)
    {
        g_source_remove(tk->tb->dnd_delay_timer);
        tk->tb->dnd_delay_timer = 0;
    }
    return;
}

/* Handler for "enter" event from taskbar button.  This indicates that the cursor position has entered the button. */
static void taskbar_button_enter(GtkWidget * widget, Task * tk)
{
    tk->entered_state = TRUE;
    if (task_button_is_really_flat(tk->tb))
        gtk_widget_set_state(widget, GTK_STATE_NORMAL);
    task_draw_label(tk);
}

/* Handler for "leave" event from taskbar button.  This indicates that the cursor position has left the button. */
static void taskbar_button_leave(GtkWidget * widget, Task * tk)
{
    tk->entered_state = FALSE;
    task_draw_label(tk);
}

/* Handler for "scroll-event" event from taskbar button. */
static gboolean taskbar_button_scroll_event(GtkWidget * widget, GdkEventScroll * event, Task * tk)
{
    GdkEventButton e;
    e.time = event->time;

    int action;

    if ((event->direction == GDK_SCROLL_UP) || (event->direction == GDK_SCROLL_LEFT))
        action = (event->state & GDK_SHIFT_MASK) ? tk->tb->shift_scroll_up_action : tk->tb->scroll_up_action;
    else
        action = (event->state & GDK_SHIFT_MASK) ? tk->tb->shift_scroll_down_action : tk->tb->scroll_down_action;

    task_action(tk, action, &e, tk, FALSE);
    
    return TRUE;
}

/******************************************************************************/

/* Task button layout message handlers. */

/* Handler for "size-allocate" event from taskbar button. */
static void taskbar_button_size_allocate(GtkWidget * btn, GtkAllocation * alloc, Task * tk)
{
    gboolean size_changed = (tk->button_alloc.width != alloc->width) || (tk->button_alloc.height != alloc->height);
    tk->button_alloc = *alloc;

    if (size_changed)
        tk->adapt_to_allocated_size_idle_cb = g_idle_add((GSourceFunc) task_adapt_to_allocated_size, tk);

    if (GTK_WIDGET_REALIZED(btn))
    {
        /* Get the coordinates of the button. */
        int x, y;
        gdk_window_get_origin(GTK_BUTTON(btn)->event_window, &x, &y);

        /* Send a NET_WM_ICON_GEOMETRY property change on the window. */
        guint32 data[4];
        data[0] = x;
        data[1] = y;
        data[2] = alloc->width;
        data[3] = alloc->height;
        XChangeProperty(GDK_DISPLAY(), tk->win,
            gdk_x11_get_xatom_by_name("_NET_WM_ICON_GEOMETRY"),
            XA_CARDINAL, 32, PropModeReplace, (guchar *) &data, 4);
    }
}

/* Handler for "size-allocate" event from taskbar button image. */
static void taskbar_image_size_allocate(GtkWidget * img, GtkAllocation * alloc, Task * tk)
{
    //int sz = alloc->width < alloc->height ? alloc->width : alloc->height;
    int sz = alloc->height;

    if (sz > tk->button_alloc.width - ICON_ONLY_EXTRA)
        sz = tk->button_alloc.width - ICON_ONLY_EXTRA;

    if (sz < 1)
        sz = 1;

    if (sz < tk->tb->icon_size)
        tk->allocated_icon_size = sz;
    else
        tk->allocated_icon_size = tk->tb->icon_size;

    tk->tb->expected_icon_size = tk->allocated_icon_size;

    if (tk->allocated_icon_size != tk->icon_size && tk->update_icon_idle_cb == 0)
        tk->update_icon_idle_cb = g_idle_add((GSourceFunc) task_update_icon_cb, tk);
}

/******************************************************************************/

/* Update style on the taskbar when created or after a configuration change. */
static void taskbar_update_style(TaskbarPlugin * tb)
{
    GtkOrientation bo = (tb->plug->panel->orientation == ORIENT_HORIZ) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    icon_grid_set_expand(tb->icon_grid, get_task_button_expandable(tb));
    icon_grid_set_geometry(tb->icon_grid, bo,
        get_task_button_max_width(tb), tb->icon_size + BUTTON_HEIGHT_EXTRA,
        tb->spacing, 0, tb->plug->panel->height);
}

/* Update style on a task button when created or after a configuration change. */
static void task_update_style(Task * tk, TaskbarPlugin * tb)
{
    gtk_widget_set_visible(tk->image, tb->show_icons);

    if (tb->show_titles) {
        if (!tk->label)
            task_build_gui_label(tb, tk);
        gtk_widget_show(tk->label);
    } else if (tk->label){
        gtk_widget_hide(tk->label);
    }

    if (task_has_visible_close_button(tk)) {
        if (!tk->button_close)
            task_build_gui_button_close(tb, tk);
        gtk_widget_show(tk->button_close);
    } else if (tk->button_close){
        gtk_widget_hide(tk->button_close);
    }

    task_button_redraw_button_state(tk, tb);
/*
    if( task_button_is_really_flat(tb) )
    {
        gtk_toggle_button_set_active((GtkToggleButton*)tk->button, FALSE);
        gtk_button_set_relief(GTK_BUTTON(tk->button), GTK_RELIEF_NONE);
    }
    else
    {
        gtk_toggle_button_set_active((GtkToggleButton*)tk->button, tk->focused);
        gtk_button_set_relief(GTK_BUTTON(tk->button), GTK_RELIEF_NORMAL);
    }
*/
    task_draw_label(tk);
}

/******************************************************************************/

/* Functions to build task gui. */

/* Build label for a task button. */
static void task_build_gui_label(TaskbarPlugin * tb, Task* tk)
{
    /* Create a label to contain the window title and add it to the box. */
    tk->label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(tk->label), 0.0, 0.5);
    gtk_label_set_ellipsize(GTK_LABEL(tk->label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(tk->container), tk->label, TRUE, TRUE, 0);
}

/* Build close button for a task button. */
static void task_build_gui_button_close(TaskbarPlugin * tb, Task* tk)
{
    if (!tk->label)
        task_build_gui_label(tb, tk);

    /* Create box for close button. */
    GtkWidget * box = gtk_vbox_new(FALSE, 1);
    gtk_container_set_border_width(GTK_CONTAINER(box), 0);

    /* Create close button. */
    tk->button_close = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
    gtk_box_pack_end(GTK_BOX(box), tk->button_close, TRUE, FALSE, 0);
    gtk_widget_set_tooltip_text (tk->button_close, _("Close window"));
    gtk_widget_show(tk->button_close);

    gtk_box_pack_end(GTK_BOX(tk->container), box, FALSE, FALSE, 0);
    gtk_widget_show(box);

    if (tb->extra_size == 0) {
        GtkRequisition r;
        gtk_widget_size_request(tk->button_close, &r);
        tb->extra_size = r.width;
    }
}

/* Build graphic elements needed for a task button. */
static void task_build_gui(TaskbarPlugin * tb, Task * tk)
{
    /* NOTE
     * 1. the extended mask is sum of taskbar and pager needs
     * see bug [ 940441 ] pager loose track of windows
     *
     * Do not change event mask to gtk windows spawned by this gtk client
     * this breaks gtk internals */
    if ( ! FBPANEL_WIN(tk->win))
        XSelectInput(GDK_DISPLAY(), tk->win, PropertyChangeMask | StructureNotifyMask);

    /* Allocate a toggle button as the top level widget. */
    tk->button = gtk_toggle_button_new();
    gtk_container_set_border_width(GTK_CONTAINER(tk->button), 0);
    gtk_drag_dest_set(tk->button, 0, NULL, 0, 0);

    /* Connect signals to the button. */
    g_signal_connect(tk->button, "button_press_event", G_CALLBACK(taskbar_button_press_event), (gpointer) tk);
    g_signal_connect(tk->button, "button_release_event", G_CALLBACK(taskbar_button_release_event), (gpointer) tk);
    g_signal_connect(G_OBJECT(tk->button), "drag-motion", G_CALLBACK(taskbar_button_drag_motion), (gpointer) tk);
    g_signal_connect(G_OBJECT(tk->button), "drag-leave", G_CALLBACK(taskbar_button_drag_leave), (gpointer) tk);
    g_signal_connect_after(G_OBJECT (tk->button), "enter", G_CALLBACK(taskbar_button_enter), (gpointer) tk);
    g_signal_connect_after(G_OBJECT (tk->button), "leave", G_CALLBACK(taskbar_button_leave), (gpointer) tk);
    g_signal_connect_after(G_OBJECT(tk->button), "scroll-event", G_CALLBACK(taskbar_button_scroll_event), (gpointer) tk);
    g_signal_connect(tk->button, "size-allocate", G_CALLBACK(taskbar_button_size_allocate), (gpointer) tk);

    /* Create a box to contain the application icon and window title. */
    tk->container = gtk_hbox_new(FALSE, 1);
    gtk_container_set_border_width(GTK_CONTAINER(tk->container), 0);

    /* Create an image to contain the application icon and add it to the box. */
    tk->image = gtk_image_new_from_pixbuf(NULL);
    gtk_misc_set_padding(GTK_MISC(tk->image), 0, 0);
    task_update_icon(tk, None);
    gtk_widget_show(tk->image);
    gtk_box_pack_start(GTK_BOX(tk->container), tk->image, FALSE, FALSE, 0);

    g_signal_connect(tk->image, "size-allocate", G_CALLBACK(taskbar_image_size_allocate), (gpointer) tk);

    if (tb->show_titles)
        task_build_gui_label(tb, tk);

    if (tb->_show_close_buttons)
        task_build_gui_button_close(tb, tk);

    /* Add the box to the button. */
    gtk_widget_show(tk->container);
    gtk_container_add(GTK_CONTAINER(tk->button), tk->container);
    gtk_container_set_border_width(GTK_CONTAINER(tk->button), 0);

    /* Add the button to the taskbar. */
    icon_grid_add(tb->icon_grid, tk->button, FALSE);
    GTK_WIDGET_UNSET_FLAGS(tk->button, GTK_CAN_FOCUS);
    GTK_WIDGET_UNSET_FLAGS(tk->button, GTK_CAN_DEFAULT);

    /* Update styles on the button. */
    task_update_style(tk, tb);

    /* Flash button for window with urgency hint. */
    if (tk->urgency)
        task_set_urgency(tk);
}

/******************************************************************************/

/* Task reordering. */

static int task_compare(Task * tk1, Task * tk2)
{
    int result = 0;
    switch (tk1->tb->_group_by)
    {
        case GROUP_BY_WORKSPACE:
        {
            int w1 = (tk1->desktop == ALL_WORKSPACES) ? 0 : (tk1->desktop + 1);
            int w2 = (tk2->desktop == ALL_WORKSPACES) ? 0 : (tk2->desktop + 1);
            result = w2 - w1;
            break;
        }
        case GROUP_BY_STATE:
        {
            int w1 = tk1->urgency * 2 + tk1->iconified;
            int w2 = tk2->urgency * 2 + tk2->iconified;
            result = w2 - w1;
            break;
        }
        case GROUP_BY_CLASS:
        {
            int w1 = tk1->res_class ? tk1->res_class->timestamp : INT_MAX;
            int w2 = tk2->res_class ? tk2->res_class->timestamp : INT_MAX;
            result = w2 - w1;
            break;
        }
    }

    if (result == 0)
        result = tk2->timestamp - tk1->timestamp;

    return result;
}

static void task_reorder(Task * tk)
{
    Task* tk_prev_old = NULL;
    Task* tk_prev_new = NULL;
    Task* tk_cursor;
    int tk_prev_new_found = 0;
    for (tk_cursor = tk->tb->task_list; tk_cursor != NULL; tk_cursor = tk_cursor->task_flink)
    {
         if (tk_cursor == tk)
             continue;
         if (!tk_prev_new_found)
         {
             //DBG("[0x%x] [\"%s\" (0x%x), \"%s\" (0x%x)] => %d\n", tk->tb, tk->name,tk, tk_cursor->name,tk_cursor, task_compare(tk, tk_cursor));

             if (task_compare(tk, tk_cursor) <= 0)
                 tk_prev_new = tk_cursor;
             else
                 tk_prev_new_found = 1;
         }
         if (tk_cursor->task_flink == tk)
             tk_prev_old = tk_cursor;

         if (tk_prev_new_found && tk_prev_old)
             break;
    }

    if (tk_prev_new) {
        if (tk_prev_old != tk_prev_new) {
            if (tk_prev_old)
                tk_prev_old->task_flink = tk->task_flink;
            else
                tk->tb->task_list = tk->task_flink;
            tk->task_flink = tk_prev_new->task_flink;
            tk_prev_new->task_flink = tk;

            //DBG("[0x%x] task \"%s\" (0x%x) moved after \"%s\" (0x%x)\n", tk->tb, tk->name, tk, tk_prev_new->name, tk_prev_new);

            icon_grid_place_child_after(tk->tb->icon_grid, tk->button, tk_prev_new->button);
        } else {
            //DBG("[0x%x] task \"%s\" (0x%x) is in rigth place\n", tk->tb, tk->name, tk);
        }
    } else {
        if (tk_prev_old)
            tk_prev_old->task_flink = tk->task_flink;
        else
            tk->tb->task_list = tk->task_flink;

        tk->task_flink = tk->tb->task_list;
        tk->tb->task_list = tk;

        //DBG("[0x%x] task \"%s\" (0x%x) moved to head\n", tk->tb, tk->name, tk);

        icon_grid_place_child_after(tk->tb->icon_grid, tk->button, NULL);
    }
}

/******************************************************************************/

/*****************************************************
 * handlers for NET actions                          *
 *****************************************************/

/* Handler for "client-list" event from root window listener. */
static void taskbar_net_client_list(GtkWidget * widget, TaskbarPlugin * tb)
{
    ENTER;

    gboolean redraw = FALSE;

    /* Get the NET_CLIENT_LIST property. */
    int client_count;
    Window * client_list = get_xaproperty(GDK_ROOT_WINDOW(), a_NET_CLIENT_LIST, XA_WINDOW, &client_count);
    if (client_list != NULL)
    {
        /* Loop over client list, correlating it with task list. */
        int i;
        for (i = 0; i < client_count; i++)
        {
            /* Search for the window in the task list.  Set up context to do an insert right away if needed. */
            Task * tk_pred = NULL;
            Task * tk_cursor;
            Task * tk = task_lookup(tb, client_list[i]);

            /* Task is already in task list. */
            if (tk != NULL)
                tk->present_in_client_list = TRUE;

            /* Task is not in task list. */
            else
            {
                /* Evaluate window state and window type to see if it should be in task list. */
                NetWMWindowType nwwt;
                NetWMState nws;
                get_net_wm_state(client_list[i], &nws);
                get_net_wm_window_type(client_list[i], &nwwt);
                if ((accept_net_wm_state(&nws))
                && (accept_net_wm_window_type(&nwwt)))
                {
                    /* Allocate and initialize new task structure. */
                    tk = g_new0(Task, 1);
                    tk->timestamp = ++tb->task_timestamp;
                    tk->focus_timestamp = 0;
                    tk->click_on = NULL;
                    tk->present_in_client_list = TRUE;
                    tk->win = client_list[i];
                    tk->tb = tb;
                    tk->name_source = None;
                    tk->image_source = None;
                    tk->iconified = (get_wm_state(tk->win) == IconicState);
                    tk->maximized = nws.maximized_vert || nws.maximized_horz;
                    tk->desktop = get_net_wm_desktop(tk->win);
                    tk->override_class_name = (char*) -1;
                    if (tb->use_urgency_hint)
                        tk->urgency = task_has_urgency(tk);
                    task_build_gui(tb, tk);
                    task_set_names(tk, None);

                    DBG("Creating task %s (0x%x)\n", tk->name, (int)tk);

                    task_set_class(tk);

                    /* Link the task structure into the task list. */
                    if (tk_pred == NULL)
                    {
                        tk->task_flink = tb->task_list;
                        tb->task_list = tk;
                    }
                    else
                    {
                        tk->task_flink = tk_pred->task_flink;
                        tk_pred->task_flink = tk;
                    }
                    task_reorder(tk);
                    icon_grid_set_visible(tb->icon_grid, tk->button, TRUE);
                    redraw = TRUE;
                }
            }
        }
        XFree(client_list);
    }

    /* Remove windows from the task list that are not present in the NET_CLIENT_LIST. */
    Task * tk_pred = NULL;
    Task * tk = tb->task_list;
    while (tk != NULL)
    {
        Task * tk_succ = tk->task_flink;
        if (tk->present_in_client_list)
        {
            tk->present_in_client_list = FALSE;
            tk_pred = tk;
        }
        else
        {
            if (tk_pred == NULL)
                tb->task_list = tk_succ;
                else tk_pred->task_flink = tk_succ;
            task_delete(tb, tk, FALSE);
            redraw = TRUE;
        }
        tk = tk_succ;
    }

    /* Redraw the taskbar. */
    if (redraw)
        taskbar_redraw(tb);

    RET();
}

/* Display given window as active. */
static void taskbar_set_active_window(TaskbarPlugin * tb, Window f)
{
    gboolean drop_old = FALSE;
    gboolean make_new = FALSE;
    Task * ctk = tb->focused;
    Task * ntk = NULL;

    /* Get the window that has focus. */
    if (f == tb->plug->panel->topxwin)
    {
        /* Taskbar window gained focus (this isn't supposed to be able to happen).  Remember current focus. */
        if (ctk != NULL)
        {
            tb->focused_previous = ctk;
            drop_old = TRUE;
        }
    }
    else
    {
        /* Identify task that gained focus. */
        tb->focused_previous = NULL;
        ntk = task_lookup(tb, f);
        if (ntk != ctk)
        {
            drop_old = TRUE;
            make_new = TRUE;
        }
    }

    //g_print("[0x%x] taskbar_set_active_window %s\n", (int) tb, ntk ? ntk->name : "(null)");

    icon_grid_defer_updates(tb->icon_grid);

    /* If our idea of the current task lost focus, update data structures. */
    if ((ctk != NULL) && (drop_old))
    {
        ctk->focused = FALSE;
        tb->focused = NULL;
    }

    /* If a task gained focus, update data structures. */
    if ((ntk != NULL) && (make_new))
    {
        ntk->focus_timestamp = ++tb->task_timestamp;
        ntk->focused = TRUE;
        tb->focused = ntk;
        recompute_group_visibility_for_class(tb, ntk->res_class);
        task_button_redraw(ntk);
    }

    if (ctk != NULL)
    {
        task_button_redraw(ctk);
    }

    if (tb->_expand_focused_group)
    {
        recompute_group_visibility_on_current_desktop(tb);
        taskbar_redraw(tb);
    }

    icon_grid_resume_updates(tb->icon_grid);
}

/* Set given desktop as current. */
static void taskbar_set_current_desktop(TaskbarPlugin * tb, int desktop)
{
    icon_grid_defer_updates(tb->icon_grid);

    /* Store the local copy of current desktops.  Redisplay the taskbar. */
    tb->current_desktop = desktop;
    //g_print("[0x%x] taskbar_set_current_desktop %d\n", (int) tb, tb->current_desktop);
    recompute_group_visibility_on_current_desktop(tb);
    taskbar_redraw(tb);

    icon_grid_resume_updates(tb->icon_grid);
}

/* Switch to deferred desktop and window. */
static gboolean taskbar_switch_desktop_and_window(TaskbarPlugin * tb)
{
    //g_print("[0x%x] taskbar_switch_desktop_and_window\n", (int) tb);

    icon_grid_defer_updates(tb->icon_grid);

    if (tb->deferred_current_desktop >= 0) {
        int desktop = tb->deferred_current_desktop;
        tb->deferred_current_desktop = -1;
        taskbar_set_current_desktop(tb, desktop);
    }

    if (tb->deferred_active_window_valid) {
        tb->deferred_active_window_valid = 0;
        taskbar_set_active_window(tb, tb->deferred_active_window);
    }

    icon_grid_resume_updates(tb->icon_grid);

    return FALSE;
}

/* Handler for "current-desktop" event from root window listener. */
static void taskbar_net_current_desktop(GtkWidget * widget, TaskbarPlugin * tb)
{
    int desktop = get_net_current_desktop();

    /* If target desktop has visible tasks, use deferred switching to redice blinking. */
    if (taskbar_has_visible_tasks_on_desktop(tb, desktop)) {
        tb->deferred_current_desktop = desktop;
        tb->deferred_desktop_switch_timer = g_timeout_add(350, (GSourceFunc) taskbar_switch_desktop_and_window, (gpointer) tb);
    } else {
        taskbar_set_current_desktop(tb, desktop);
    }
}

/* Handler for "number-of-desktops" event from root window listener. */
static void taskbar_net_number_of_desktops(GtkWidget * widget, TaskbarPlugin * tb)
{
    /* Store the local copy of number of desktops.  Recompute the popup menu and redisplay the taskbar. */
    tb->number_of_desktops = get_net_number_of_desktops();
    taskbar_make_menu(tb);
    taskbar_redraw(tb);
}

/* Handler for "active-window" event from root window listener. */
static void taskbar_net_active_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    /* Get active window. */
    Window * p = get_xaproperty(GDK_ROOT_WINDOW(), a_NET_ACTIVE_WINDOW, XA_WINDOW, 0);
    Window w = p ? *p : 0;
    XFree(p);

    //g_print("[0x%x] net_active_window %d\n", (int) tb, (int)w);

    /* If there is no deferred desktop switching, set active window directly. */
    if (tb->deferred_current_desktop < 0) {
        taskbar_set_active_window(tb, w);
    } else {
        /* Add window to the deferred switching. */
        tb->deferred_active_window_valid = 1;
        tb->deferred_active_window = w;
        /* If window is not null, do referred switching now. */
        if (w) {
            taskbar_switch_desktop_and_window(tb);
        }
    }
}

/* Determine if the "urgency" hint is set on a window. */
static gboolean task_has_urgency(Task * tk)
{
    gboolean result = FALSE;
    XWMHints * hints = (XWMHints *) get_xaproperty(tk->win, XA_WM_HINTS, XA_WM_HINTS, 0);
    if (hints != NULL)
    {
        if (hints->flags & XUrgencyHint)
            result = TRUE;
        XFree(hints);
    }
    return result;
}

/* Handler for desktop_name event from window manager. */
static void taskbar_net_desktop_names(FbEv * fbev, TaskbarPlugin * tb)
{
    if (tb->desktop_names != NULL)
        g_strfreev(tb->desktop_names),
        tb->desktop_names;

    /* Get the NET_DESKTOP_NAMES property. */
    tb->desktop_names = get_utf8_property_list(GDK_ROOT_WINDOW(), a_NET_DESKTOP_NAMES, &tb->number_of_desktop_names);
}

static void task_update_grouping(Task * tk, int group_by)
{
    ENTER;
    DBG("group_by = %d, tb->_group_by = %d\n", group_by, tk->tb->_group_by);
    if (tk->tb->_group_by == group_by || group_by < 0)
    {
        task_set_class(tk);
        task_reorder(tk);
        taskbar_redraw(tk->tb);
    }
    RET();
}


/* Handle PropertyNotify event.
 * http://tronche.com/gui/x/icccm/
 * http://standards.freedesktop.org/wm-spec/wm-spec-1.4.html */
static void taskbar_property_notify_event(TaskbarPlugin *tb, XEvent *ev)
{
    /* State may be PropertyNewValue, PropertyDeleted. */
    if (((XPropertyEvent*) ev)->state == PropertyNewValue)
    {
        Atom at = ev->xproperty.atom;
        Window win = ev->xproperty.window;
        if (win != GDK_ROOT_WINDOW())
        {
            /* Look up task structure by X window handle. */
            Task * tk = task_lookup(tb, win);
            if (tk != NULL)
            {
                /* Install an error handler that ignores BadWindow.
                 * We frequently get a PropertyNotify event on deleted windows. */
                XErrorHandler previous_error_handler = XSetErrorHandler(panel_handle_x_error_swallow_BadWindow_BadDrawable);

                /* Dispatch on atom. */
                if (at == a_NET_WM_DESKTOP)
                {
                    /* Window changed desktop. */
                    tk->desktop = get_net_wm_desktop(win);
                    task_update_grouping(tk, GROUP_BY_WORKSPACE);
                    taskbar_redraw(tb);
                }
                else if ((at == XA_WM_NAME) || (at == a_NET_WM_NAME) || (at == a_NET_WM_VISIBLE_NAME))
                {
                    /* Window changed name. */
                    task_set_names(tk, at);
                    if (tk->res_class != NULL)
                    {
                        /* A change to the window name may change the visible name of the class. */
                        recompute_group_visibility_for_class(tb, tk->res_class);
                        if (tk->res_class->visible_task != NULL)
                            task_draw_label(tk->res_class->visible_task);
                    }
                }
                else if (at == XA_WM_CLASS)
                {
                    /* Window changed class. */
                    task_update_grouping(tk, GROUP_BY_CLASS);
                }
                else if (at == a_WM_STATE)
                {
                    /* Window changed state. */
                    tk->iconified = (get_wm_state(win) == IconicState);
                    /* Do not update task label, if we a waiting for deferred desktop switching. */
                    if (tb->deferred_current_desktop < 0)
                    {
                       task_draw_label(tk);
                    }
                    task_update_grouping(tk, GROUP_BY_STATE);
                }
                else if (at == XA_WM_HINTS)
                {
                    /* Window changed "window manager hints".
                     * Some windows set their WM_HINTS icon after mapping. */
                    task_update_icon(tk, XA_WM_HINTS);

                    if (tb->use_urgency_hint)
                    {
                        tk->urgency = task_has_urgency(tk);
                        if (tk->urgency)
                            task_set_urgency(tk);
                        else
                            task_clear_urgency(tk);
                        task_update_grouping(tk, GROUP_BY_STATE);
                    }
                }
                else if (at == a_NET_WM_STATE)
                {
                    /* Window changed EWMH state. */
                    NetWMState nws;
                    get_net_wm_state(tk->win, &nws);
                    if ( ! accept_net_wm_state(&nws))
                    {
                        task_delete(tb, tk, TRUE);
                        taskbar_redraw(tb);
                    }
                    tk->maximized = nws.maximized_vert || nws.maximized_horz;
                }
                else if (at == a_NET_WM_ICON)
                {
                    /* Window changed EWMH icon. */
                    task_update_icon(tk, a_NET_WM_ICON);
                }
                else if (at == a_NET_WM_WINDOW_TYPE)
                {
                    /* Window changed EWMH window type. */
                    NetWMWindowType nwwt;
                    get_net_wm_window_type(tk->win, &nwwt);
                    if ( ! accept_net_wm_window_type(&nwwt))
                    {
                        task_delete(tb, tk, TRUE);
                        taskbar_redraw(tb);
                    }
                }
                XSetErrorHandler(previous_error_handler);
            }
        }
    }
}

/* GDK event filter. */
static GdkFilterReturn taskbar_event_filter(XEvent * xev, GdkEvent * event, TaskbarPlugin * tb)
{
    /* Look for PropertyNotify events and update state. */
    if (xev->type == PropertyNotify)
        taskbar_property_notify_event(tb, xev);
    return GDK_FILTER_CONTINUE;
}

/******************************************************************************/

/* Task button context menu handlers */

static void menu_raise_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    if ((tb->menutask->desktop != ALL_WORKSPACES) && (tb->menutask->desktop != tb->current_desktop))
        Xclimsg(GDK_ROOT_WINDOW(), a_NET_CURRENT_DESKTOP, tb->menutask->desktop, 0, 0, 0, 0);
    XMapRaised(GDK_DISPLAY(), tb->menutask->win);
    task_group_menu_destroy(tb);
}

static void menu_restore_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_maximize(tb->menutask);
    task_group_menu_destroy(tb);
}

static void menu_maximize_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_maximize(tb->menutask);
    task_group_menu_destroy(tb);
}

static void menu_iconify_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_iconify(tb->menutask);
    task_group_menu_destroy(tb);
}

static void menu_move_to_workspace(GtkWidget * widget, TaskbarPlugin * tb)
{
    int num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "num"));
    Xclimsg(tb->menutask->win, a_NET_WM_DESKTOP, num, 0, 0, 0, 0);
    task_group_menu_destroy(tb);
}

static void menu_ungroup_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_set_override_class(tb->menutask, NULL);
    task_group_menu_destroy(tb);
}

static void menu_move_to_group(GtkWidget * widget, TaskbarPlugin * tb)
{
    TaskClass * tc = (TaskClass *)(g_object_get_data(G_OBJECT(widget), "res_class"));
    if (tc && tc->res_class)
    {
        char * name = g_strdup(tc->res_class);
        task_set_override_class(tb->menutask, name);
        g_free(name);
    }
    task_group_menu_destroy(tb);
}

static void menu_expand_group_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    TaskClass * tc = tb->menutask->res_class;
    if (tc)
    {
        tc->expand = TRUE;
        tc->manual_expand_state = TRUE;

        icon_grid_defer_updates(tb->icon_grid);
        recompute_group_visibility_on_current_desktop(tb);
        taskbar_redraw(tb);
        icon_grid_resume_updates(tb->icon_grid);
    }
    task_group_menu_destroy(tb);
}

static void menu_shrink_group_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    TaskClass * tc = tb->menutask->res_class;
    if (tc)
    {
        tc->expand = FALSE;
        tc->manual_expand_state = TRUE;

        icon_grid_defer_updates(tb->icon_grid);
        recompute_group_visibility_on_current_desktop(tb);
        taskbar_redraw(tb);
        icon_grid_resume_updates(tb->icon_grid);
    }
    task_group_menu_destroy(tb);
}

static void menu_close_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_close(tb->menutask);
    task_group_menu_destroy(tb);
}

/******************************************************************************/

/* Context menu adjust functions. */

static void task_adjust_menu_workspace_callback(GtkWidget *widget, gpointer data)
{
    Task* tk = (Task*)data;
    int num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "num"));
    gtk_widget_set_sensitive(widget, num != tk->desktop);
}

static void task_adjust_menu_move_to_group(Task * tk)
{
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(tk->tb->move_to_group_menuitem), NULL);

    GtkWidget * move_to_group_menu = gtk_menu_new();

    TaskClass * tc;
    for (tc = tk->tb->res_class_list; tc != NULL; tc = tc->res_class_flink)
    {
        if (tc->visible_count && tk->res_class != tc)
        {
            gchar * label = g_strdup((tc->res_class && strlen(tc->res_class) > 0) ? tc->res_class : _("(unnamed)"));
            GtkWidget * mi = gtk_menu_item_new_with_label(label);
            g_free(label);

            g_object_set_data(G_OBJECT(mi), "res_class", tc);
            g_signal_connect(mi, "activate", G_CALLBACK(menu_move_to_group), tk->tb);
            gtk_menu_shell_append(GTK_MENU_SHELL(move_to_group_menu), mi);

            gtk_widget_set_visible(GTK_WIDGET(mi), TRUE);
        }
    }

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(tk->tb->move_to_group_menuitem), move_to_group_menu);
}

typedef struct {
    gboolean prev_is_separator;
    GtkWidget * last_visible;
} _AdjustSeparatorsData;

static void adjust_separators_callback(GtkWidget * widget, gpointer d)
{
    _AdjustSeparatorsData * data = (_AdjustSeparatorsData *) d;
    
    gboolean is_separator = GTK_IS_SEPARATOR_MENU_ITEM(widget);

    if (gtk_widget_get_visible(GTK_WIDGET(widget))) {
        if (data->prev_is_separator && is_separator) {
            gtk_widget_set_visible(GTK_WIDGET(widget), FALSE);
        } else {
            data->last_visible = widget;
            data->prev_is_separator = is_separator;
        }
    } else {
        if (!data->prev_is_separator && is_separator) {
            gtk_widget_set_visible(GTK_WIDGET(widget), TRUE);
            data->last_visible = widget;
            data->prev_is_separator = is_separator;
        }
    }
}

static void adjust_separators(GtkWidget * menu)
{
    _AdjustSeparatorsData data;
    data.prev_is_separator = TRUE;
    data.last_visible = NULL;
    gtk_container_foreach(GTK_CONTAINER(menu), adjust_separators_callback, &data);
    if (data.last_visible)
        adjust_separators_callback(data.last_visible, &data);
}

static void task_adjust_menu(Task * tk, gboolean from_popup_menu)
{
    TaskbarPlugin * tb = tk->tb;

    if (tb->workspace_submenu) {
        gtk_container_foreach(GTK_CONTAINER(tb->workspace_submenu), task_adjust_menu_workspace_callback, tk);
    }

    gboolean manual_grouping = tb->manual_grouping && tb->grouped_tasks;
    if (manual_grouping)
        task_adjust_menu_move_to_group(tk);
    gtk_widget_set_visible(GTK_WIDGET(tb->move_to_group_menuitem), manual_grouping);
    gtk_widget_set_visible(GTK_WIDGET(tb->ungroup_menuitem), manual_grouping && tk->res_class && tk->res_class->visible_count > 1);

    gtk_widget_set_visible(GTK_WIDGET(tb->expand_group_menuitem),
        manual_grouping && tk->res_class && task_class_is_grouped(tb, tk->res_class));
    gtk_widget_set_visible(GTK_WIDGET(tk->tb->shrink_group_menuitem),
        manual_grouping && tk->res_class && !task_class_is_grouped(tb, tk->res_class));

    gtk_widget_set_visible(GTK_WIDGET(tb->maximize_menuitem), !tk->maximized);
    gtk_widget_set_visible(GTK_WIDGET(tb->restore_menuitem), tk->maximized);

    gtk_widget_set_sensitive(GTK_WIDGET(tb->iconify_menuitem), !tk->iconified);

    gtk_widget_set_visible(GTK_WIDGET(tb->title_menuitem), from_popup_menu);
    if (from_popup_menu) {
        gtk_widget_set_sensitive(GTK_WIDGET(tb->title_menuitem), FALSE);
        gtk_menu_item_set_use_underline(GTK_MENU_ITEM(tb->title_menuitem), FALSE);
        gtk_menu_item_set_label(GTK_MENU_ITEM(tb->title_menuitem), tk->name);
    }

    adjust_separators(tb->menu);
}

/******************************************************************************/

/* Functions to build task context menu. */

/* Helper function to create menu items for taskbar_make_menu() */
static GtkWidget * create_menu_item(TaskbarPlugin * tb, char * name, GCallback activate_cb, GtkWidget ** menuitem)
{
    GtkWidget * mi = gtk_menu_item_new_with_mnemonic(name);
    gtk_menu_shell_append(GTK_MENU_SHELL(tb->menu), mi);
    if (activate_cb)
        g_signal_connect(G_OBJECT(mi), "activate", activate_cb, tb);
    if (menuitem)
        *menuitem = mi;
    return mi;
}

/* Make right-click menu for task buttons.
 * This depends on number of desktops and edge. */
static void taskbar_make_menu(TaskbarPlugin * tb)
{
    /* Deallocate old menu if present. */
    if (tb->menu != NULL)
        gtk_widget_destroy(tb->menu);

    /* Allocate menu. */
    GtkWidget * menu = gtk_menu_new();
    tb->menu = menu;

    GtkWidget * mi;
        
    /* Create menu items. */

    create_menu_item(tb, _("_Raise"), (GCallback) menu_raise_window, NULL);
    create_menu_item(tb, _("R_estore"), (GCallback) menu_restore_window, &tb->restore_menuitem);
    create_menu_item(tb, _("Ma_ximize"), (GCallback) menu_maximize_window, &tb->maximize_menuitem);
    create_menu_item(tb, _("Ico_nify"), (GCallback) menu_iconify_window, &tb->iconify_menuitem);

    /* If multiple desktops are supported, add menu items to select them. */
    tb->workspace_submenu = NULL;
    if (tb->number_of_desktops > 1)
    {
        /* Allocate submenu. */
        GtkWidget * workspace_menu = gtk_menu_new();

        /* Loop over all desktops. */
        int i;
        for (i = 1; i <= tb->number_of_desktops; i++)
        {
            gchar * deflabel = g_strdup_printf( "Workspace %d", i);
            gchar * label = taskbar_get_desktop_name(tb, i - 1, deflabel);
            mi = gtk_menu_item_new_with_label(label);
            g_free(label);
            g_free(deflabel);

            /* Set the desktop number as a property on the menu item. */
            g_object_set_data(G_OBJECT(mi), "num", GINT_TO_POINTER(i - 1));
            g_signal_connect(mi, "activate", G_CALLBACK(menu_move_to_workspace), tb);
            gtk_menu_shell_append(GTK_MENU_SHELL(workspace_menu), mi);
        }

        /* Add a separator. */
        gtk_menu_shell_append(GTK_MENU_SHELL(workspace_menu), gtk_separator_menu_item_new());

        /* Add "move to all workspaces" item.  This causes the window to be visible no matter what desktop is active. */
        mi = gtk_menu_item_new_with_mnemonic(_("_All workspaces"));
        g_object_set_data(G_OBJECT(mi), "num", GINT_TO_POINTER(ALL_WORKSPACES));
        g_signal_connect(mi, "activate", G_CALLBACK(menu_move_to_workspace), tb);
        gtk_menu_shell_append(GTK_MENU_SHELL(workspace_menu), mi);

        /* Add Move to Workspace menu item as a submenu. */
        mi = create_menu_item(tb, _("_Move to Workspace"), NULL, NULL);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), workspace_menu);

        tb->workspace_submenu = workspace_menu;
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    
    create_menu_item(tb, _("_Ungroup"), (GCallback) menu_ungroup_window, &tb->ungroup_menuitem);
    create_menu_item(tb, _("M_ove to Group"), NULL, &tb->move_to_group_menuitem);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    
    create_menu_item(tb, _("Expand _Group"), (GCallback) menu_expand_group_window, &tb->expand_group_menuitem);
    create_menu_item(tb, _("Shrink _Group"), (GCallback) menu_shrink_group_window, &tb->shrink_group_menuitem);

    /* Add Close menu item.  By popular demand, we place this menu item closest to the cursor. */
    mi = gtk_menu_item_new_with_mnemonic (_("_Close Window"));
    if (tb->plug->panel->edge != EDGE_BOTTOM)
    {
        gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), mi);
    }
    else
    {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    }
    g_signal_connect(G_OBJECT(mi), "activate", (GCallback)menu_close_window, tb);

    /* Add window title menu item and separator. */

    mi = gtk_separator_menu_item_new();
    gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), mi);

    mi = gtk_menu_item_new_with_mnemonic("");
    gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), mi);
    tb->title_menuitem = mi;

    gtk_widget_show_all(menu);
}

/******************************************************************************/

/* Handler for "window-manager-changed" event. */
static void taskbar_window_manager_changed(GdkScreen * screen, TaskbarPlugin * tb)
{
    /* Force re-evaluation of use_net_active. */
    tb->net_active_checked = FALSE;
}

/* Build graphic elements needed for the taskbar. */
static void taskbar_build_gui(Plugin * p)
{
    TaskbarPlugin * tb = (TaskbarPlugin *) p->priv;

    /* Set up style for taskbar. */
    gtk_rc_parse_string(taskbar_rc);

    /* Allocate top level widget and set into Plugin widget pointer. */
    p->pwid = gtk_event_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(p->pwid), 0);
    GTK_WIDGET_SET_FLAGS(p->pwid, GTK_NO_WINDOW);
    gtk_widget_set_name(p->pwid, "taskbar");

    /* Make container for task buttons as a child of top level widget. */
    GtkOrientation bo = (tb->plug->panel->orientation == ORIENT_HORIZ) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    tb->icon_grid = icon_grid_new(p->panel, p->pwid, bo, tb->task_width_max, tb->icon_size, tb->spacing, 0, p->panel->height);
    icon_grid_set_expand(tb->icon_grid, get_task_button_expandable(tb));
    taskbar_update_style(tb);

    /* Add GDK event filter. */
    gdk_window_add_filter(NULL, (GdkFilterFunc) taskbar_event_filter, tb);

    /* Connect signal to receive mouse events on the unused portion of the taskbar. */
    g_signal_connect(p->pwid, "button-press-event", G_CALLBACK(plugin_button_press_event), p);

    /* Connect signals to receive root window events and initialize root window properties. */
    tb->number_of_desktops = get_net_number_of_desktops();
    tb->current_desktop = get_net_current_desktop();
    g_signal_connect(G_OBJECT(fbev), "current_desktop", G_CALLBACK(taskbar_net_current_desktop), (gpointer) tb);
    g_signal_connect(G_OBJECT(fbev), "active_window", G_CALLBACK(taskbar_net_active_window), (gpointer) tb);
    g_signal_connect(G_OBJECT(fbev), "number_of_desktops", G_CALLBACK(taskbar_net_number_of_desktops), (gpointer) tb);
    g_signal_connect(G_OBJECT(fbev), "desktop_names", G_CALLBACK(taskbar_net_desktop_names), (gpointer) tb);
    g_signal_connect(G_OBJECT(fbev), "client_list", G_CALLBACK(taskbar_net_client_list), (gpointer) tb);

    /* Make right-click menu for task buttons.
     * It is retained for the life of the taskbar and will be shown as needed.
     * Number of desktops and edge is needed for this operation. */
    taskbar_make_menu(tb);

    /* Connect a signal to be notified when the window manager changes.  This causes re-evaluation of the "use_net_active" status. */
    g_signal_connect(gtk_widget_get_screen(p->pwid), "window-manager-changed", G_CALLBACK(taskbar_window_manager_changed), tb);
}

static void taskbar_config_updated(TaskbarPlugin * tb)
{
    if (!tb->show_iconified)
        tb->show_mapped = TRUE;

    tb->grouped_tasks = tb->mode == MODE_GROUP;
    tb->single_window = tb->mode == MODE_SINGLE_WINDOW;

    int group_by = (tb->grouped_tasks) ? tb->group_by : GROUP_BY_NONE;

    tb->show_icons  = tb->show_icons_titles != SHOW_TITLES;
    tb->show_titles = tb->show_icons_titles != SHOW_ICONS;

    tb->rebuild_gui = tb->_mode != tb->mode;
    tb->rebuild_gui |= tb->show_all_desks_prev_value != tb->show_all_desks;
    tb->rebuild_gui |= tb->_group_shrink_threshold != tb->group_shrink_threshold;
    tb->rebuild_gui |= tb->_group_by != group_by;
    tb->rebuild_gui |= tb->show_iconified_prev != tb->show_iconified;
    tb->rebuild_gui |= tb->show_mapped_prev != tb->show_mapped;

    if (tb->rebuild_gui) {
        tb->_mode = tb->mode;
        tb->show_all_desks_prev_value = tb->show_all_desks;
        tb->_group_shrink_threshold = tb->group_shrink_threshold;
        tb->_group_by = group_by;
        tb->show_iconified_prev = tb->show_iconified;
        tb->show_mapped_prev = tb->show_mapped;
    }

    tb->_show_close_buttons = tb->show_close_buttons && !(tb->grouped_tasks && tb->_group_shrink_threshold == 1);

    if (tb->_expand_focused_group != tb->expand_focused_group)
    {
        tb->_expand_focused_group = tb->expand_focused_group;
        recompute_group_visibility_on_current_desktop(tb);
        taskbar_redraw(tb);
    }

}

/* Plugin constructor. */
static int taskbar_constructor(Plugin * p, char ** fp)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    TaskbarPlugin * tb = g_new0(TaskbarPlugin, 1);
    tb->plug = p;
    p->priv = tb;

    /* Initialize to defaults. */
    tb->icon_size         = p->panel->icon_size;
    tb->tooltips          = TRUE;
    tb->show_icons_titles = SHOW_BOTH;
    tb->show_all_desks    = FALSE;
    tb->show_mapped       = TRUE;
    tb->show_iconified    = TRUE;
    tb->task_width_max    = TASK_WIDTH_MAX;
    tb->spacing           = 1;
    tb->use_urgency_hint  = TRUE;
    tb->mode              = MODE_CLASSIC;
    tb->group_shrink_threshold = 1;
    tb->group_by          = GROUP_BY_CLASS;
    tb->manual_grouping   = TRUE;
    tb->expand_focused_group = FALSE;
    tb->show_close_buttons = FALSE;

    tb->button1_action    = ACTION_RAISEICONIFY;
    tb->button2_action    = ACTION_SHADE;
    tb->button3_action    = ACTION_MENU;
    tb->scroll_up_action  = ACTION_PREV_WINDOW;
    tb->scroll_down_action = ACTION_NEXT_WINDOW;

    tb->shift_button1_action    = ACTION_ICONIFY;
    tb->shift_button2_action    = ACTION_MAXIMIZE;
    tb->shift_button3_action    = ACTION_CLOSE;
    tb->shift_scroll_up_action  = ACTION_PREV_WINDOW_IN_CURRENT_GROUP;
    tb->shift_scroll_down_action = ACTION_NEXT_WINDOW_IN_CURRENT_GROUP;

    tb->grouped_tasks     = FALSE;
    tb->single_window     = FALSE;
    tb->group_side_by_side = FALSE;
    tb->show_icons        = TRUE;
    tb->show_titles       = TRUE;
    tb->show_all_desks_prev_value = FALSE;
    tb->_show_close_buttons = FALSE;
    tb->extra_size        = 0;

    tb->a_OB_WM_STATE_UNDECORATED  = XInternAtom(GDK_DISPLAY(), "_OB_WM_STATE_UNDECORATED", False);

    tb->workspace_submenu = NULL;
    tb->restore_menuitem  = NULL;
    tb->maximize_menuitem = NULL;
    tb->iconify_menuitem  = NULL;
    tb->ungroup_menuitem  = NULL;
    tb->move_to_group_menuitem  = NULL;
    tb->title_menuitem    = NULL;

    tb->desktop_names = NULL;
    tb->number_of_desktop_names = 0;

    tb->deferred_desktop_switch_timer = 0;
    tb->deferred_current_desktop = -1;
    tb->deferred_active_window_valid = 0;

    tb->task_timestamp = 0;

    /* Process configuration file. */
    line s;
    s.len = 256;
    if( fp )
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END) {
            if (s.type == LINE_NONE) {
                ERR( "taskbar: illegal token %s\n", s.str);
                return 0;
            }
            if (s.type == LINE_VAR)
            {
                if (g_ascii_strcasecmp(s.t[0], "tooltips") == 0)
                    tb->tooltips = str2num(bool_pair, s.t[1], tb->tooltips);
                else if (g_ascii_strcasecmp(s.t[0], "IconsOnly") == 0)			/* For backward compatibility */
                    ;
                else if (g_ascii_strcasecmp(s.t[0], "AcceptSkipPager") == 0)		/* For backward compatibility */
                    ;
                else if (g_ascii_strcasecmp(s.t[0], "ShowIconified") == 0)
                    tb->show_iconified = str2num(bool_pair, s.t[1], tb->show_iconified);
                else if (g_ascii_strcasecmp(s.t[0], "ShowMapped") == 0)
                    tb->show_mapped = str2num(bool_pair, s.t[1], tb->show_mapped);
                else if (g_ascii_strcasecmp(s.t[0], "ShowAllDesks") == 0)
                    tb->show_all_desks = str2num(bool_pair, s.t[1], tb->show_all_desks);
                else if (g_ascii_strcasecmp(s.t[0], "MaxTaskWidth") == 0)
                    tb->task_width_max = atoi(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "spacing") == 0)
                    tb->spacing = atoi(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "UseMouseWheel") == 0)              /* For backward compatibility */
                    ;
                else if (g_ascii_strcasecmp(s.t[0], "UseUrgencyHint") == 0)
                    tb->use_urgency_hint = str2num(bool_pair, s.t[1], tb->use_urgency_hint);
                else if (g_ascii_strcasecmp(s.t[0], "FlatButton") == 0)
                    tb->flat_button = str2num(bool_pair, s.t[1], tb->flat_button);
                else if (g_ascii_strcasecmp(s.t[0], "GroupedTasks") == 0)		/* For backward compatibility */
                    ;
                else if (g_ascii_strcasecmp(s.t[0], "Mode") == 0)
                    tb->mode = str2num(mode_pair, s.t[1], tb->mode);
                else if (g_ascii_strcasecmp(s.t[0], "GroupThreshold") == 0)
                    tb->group_shrink_threshold = atoi(s.t[1]);				/* For backward compatibility */
                else if (g_ascii_strcasecmp(s.t[0], "GroupShrinkThreshold") == 0)
                    tb->group_shrink_threshold = atoi(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "GroupBy") == 0)
                    tb->group_by = str2num(group_by_pair, s.t[1], tb->group_by);
                else if (g_ascii_strcasecmp(s.t[0], "ManualGrouping") == 0)
                    tb->manual_grouping = str2num(bool_pair, s.t[1], tb->manual_grouping);
                else if (g_ascii_strcasecmp(s.t[0], "ExpandFocusedGroup") == 0)
                    tb->expand_focused_group = str2num(bool_pair, s.t[1], tb->expand_focused_group);
                else if (g_ascii_strcasecmp(s.t[0], "ShowIconsTitles") == 0)
                    tb->show_icons_titles = str2num(show_pair, s.t[1], tb->show_icons_titles);
                else if (g_ascii_strcasecmp(s.t[0], "ShowCloseButtons") == 0)
                    tb->show_close_buttons = str2num(bool_pair, s.t[1], tb->show_close_buttons);
                else if (g_ascii_strcasecmp(s.t[0], "SelfGroupSingleWindow") == 0)
                    tb->group_shrink_threshold = str2num(bool_pair, s.t[1], 0) ? 1 : 2;        /* For backward compatibility */
                else if (g_ascii_strcasecmp(s.t[0], "Button1Action") == 0)
                    tb->button1_action = str2num(action_pair, s.t[1], tb->button1_action);
                else if (g_ascii_strcasecmp(s.t[0], "Button2Action") == 0)
                    tb->button2_action = str2num(action_pair, s.t[1], tb->button2_action);
                else if (g_ascii_strcasecmp(s.t[0], "Button3Action") == 0)
                    tb->button3_action = str2num(action_pair, s.t[1], tb->button3_action);
                else if (g_ascii_strcasecmp(s.t[0], "ScrollUpAction") == 0)
                    tb->scroll_up_action = str2num(action_pair, s.t[1], tb->scroll_up_action);
                else if (g_ascii_strcasecmp(s.t[0], "ScrollDownAction") == 0)
                    tb->scroll_down_action = str2num(action_pair, s.t[1], tb->scroll_down_action);
                else if (g_ascii_strcasecmp(s.t[0], "ShiftButton1Action") == 0)
                    tb->shift_button1_action = str2num(action_pair, s.t[1], tb->shift_button1_action);
                else if (g_ascii_strcasecmp(s.t[0], "ShiftButton2Action") == 0)
                    tb->shift_button2_action = str2num(action_pair, s.t[1], tb->shift_button2_action);
                else if (g_ascii_strcasecmp(s.t[0], "ShiftButton3Action") == 0)
                    tb->shift_button3_action = str2num(action_pair, s.t[1], tb->shift_button3_action);
                else if (g_ascii_strcasecmp(s.t[0], "ShiftScrollUpAction") == 0)
                    tb->shift_scroll_up_action = str2num(action_pair, s.t[1], tb->shift_scroll_up_action);
                else if (g_ascii_strcasecmp(s.t[0], "ShiftScrollDownAction") == 0)
                    tb->shift_scroll_down_action = str2num(action_pair, s.t[1], tb->shift_scroll_down_action);
                else
                    ERR( "taskbar: unknown var %s\n", s.t[0]);
            }
            else
            {
                ERR( "taskbar: illegal in this context %s\n", s.str);
                return 0;
            }
        }
    }

    taskbar_config_updated(tb);

    taskbar_net_desktop_names(NULL, tb);

    /* Build the graphic elements. */
    taskbar_build_gui(p);

    taskbar_net_desktop_names(NULL, tb);

    /* Fetch the client list and redraw the taskbar.  Then determine what window has focus. */
    taskbar_net_client_list(NULL, tb);
    taskbar_net_active_window(NULL, tb);

    taskbar_update_style(tb);

    return 1;
}

/* Plugin destructor. */
static void taskbar_destructor(Plugin * p)
{
    TaskbarPlugin * tb = (TaskbarPlugin *) p->priv;

    if (tb->deferred_desktop_switch_timer != 0)
        g_source_remove(tb->deferred_desktop_switch_timer);

    /* Remove GDK event filter. */
    gdk_window_remove_filter(NULL, (GdkFilterFunc) taskbar_event_filter, tb);

    /* Remove root window signal handlers. */
    g_signal_handlers_disconnect_by_func(fbev, taskbar_net_current_desktop, tb);
    g_signal_handlers_disconnect_by_func(fbev, taskbar_net_active_window, tb);
    g_signal_handlers_disconnect_by_func(fbev, taskbar_net_number_of_desktops, tb);
    g_signal_handlers_disconnect_by_func(fbev, taskbar_net_client_list, tb);

    /* Remove "window-manager-changed" handler. */
    g_signal_handlers_disconnect_by_func(gtk_widget_get_screen(p->pwid), taskbar_window_manager_changed, tb);

    if (tb->desktop_names != NULL)
        g_strfreev(tb->desktop_names);

    /* Deallocate task list. */
    while (tb->task_list != NULL)
        task_delete(tb, tb->task_list, TRUE);

    /* Deallocate class list. */
    while (tb->res_class_list != NULL)
    {
        TaskClass * tc = tb->res_class_list;
        tb->res_class_list = tc->res_class_flink;
        g_free(tc->res_class);
        g_free(tc);
    }

    /* Deallocate other memory. */
    gtk_widget_destroy(tb->menu);
    g_free(tb);
}

/* Callback from configuration dialog mechanism to apply the configuration. */
static void taskbar_apply_configuration(Plugin * p)
{
    TaskbarPlugin * tb = (TaskbarPlugin *) p->priv;

    taskbar_config_updated(tb);

    if (tb->rebuild_gui) {
        tb->rebuild_gui = FALSE;

        /* Deallocate task list. */
        while (tb->task_list != NULL)
            task_delete(tb, tb->task_list, TRUE);

        /* Fetch the client list and redraw the taskbar.  Then determine what window has focus. */
        taskbar_net_client_list(NULL, tb);
        taskbar_net_active_window(NULL, tb);
    }

    /* Update style on taskbar. */
    taskbar_update_style(tb);

    /* Update styles on each button. */
    Task * tk;
    for (tk = tb->task_list; tk != NULL; tk = tk->task_flink)
        task_update_style(tk, tb);

    /* Refetch the client list and redraw. */
    recompute_group_visibility_on_current_desktop(tb);
    taskbar_net_client_list(NULL, tb);
}

/* Display the configuration dialog. */
static void taskbar_configure(Plugin * p, GtkWindow * parent)
{
    const char* actions = _("|None|Show menu|Close|Raise/Iconify|Iconify|Maximize|Shade|Undecorate|Fullscreen|Stick|Show window list|Show similar window list|Next window|Previous window|Next window in current group|Previous window in current group|Next window in pointed group|Previous window in pointed group");
    char* button1_action = g_strdup_printf("%s%s", _("|Left button"), actions);
    char* button2_action = g_strdup_printf("%s%s", _("|Middle button"), actions);
    char* button3_action = g_strdup_printf("%s%s", _("|Right button"), actions);
    char* scroll_up_action = g_strdup_printf("%s%s", _("|Scroll up"), actions);
    char* scroll_down_action = g_strdup_printf("%s%s", _("|Scroll down"), actions);
    char* shift_button1_action = g_strdup_printf("%s%s", _("|Shift + Left button"), actions);
    char* shift_button2_action = g_strdup_printf("%s%s", _("|Shift + Middle button"), actions);
    char* shift_button3_action = g_strdup_printf("%s%s", _("|Shift + Right button"), actions);
    char* shift_scroll_up_action = g_strdup_printf("%s%s", _("|Shift + Scroll up"), actions);
    char* shift_scroll_down_action = g_strdup_printf("%s%s", _("|Shift + Scroll down"), actions);


    TaskbarPlugin * tb = (TaskbarPlugin *) p->priv;
    GtkWidget* dlg = create_generic_config_dlg(
        _(p->class->name),
        GTK_WIDGET(parent),
        (GSourceFunc) taskbar_apply_configuration, (gpointer) p,
        _("Appearance"), (gpointer)NULL, (GType)CONF_TYPE_BEGIN_PAGE,

        _("|Show:|Icons only|Titles only|Icons and titles"), (gpointer)&tb->show_icons_titles, (GType)CONF_TYPE_ENUM,
        _("Show tooltips"), (gpointer)&tb->tooltips, (GType)CONF_TYPE_BOOL,
        _("Show close buttons"), (gpointer)&tb->show_close_buttons, (GType)CONF_TYPE_BOOL,
        _("Flat buttons"), (gpointer)&tb->flat_button, (GType)CONF_TYPE_BOOL,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("Maximum width of task button"), (gpointer)&tb->task_width_max, (GType)CONF_TYPE_INT,
        _("Spacing"), (gpointer)&tb->spacing, (GType)CONF_TYPE_INT,
        "", 0, (GType)CONF_TYPE_END_TABLE,

        _("Behavior"), (gpointer)NULL, (GType)CONF_TYPE_BEGIN_PAGE,

        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("|Mode|Classic|Group windows|Show only active window"), (gpointer)&tb->mode, (GType)CONF_TYPE_ENUM,
        _("|Group by|None|Window class|Workspace|Window state"), (gpointer)&tb->group_by, (GType)CONF_TYPE_ENUM,
        _("Group shrink threshold"), (gpointer)&tb->group_shrink_threshold, (GType)CONF_TYPE_INT,
        _("Expand focused group"), (gpointer)&tb->expand_focused_group, (GType)CONF_TYPE_BOOL,
        _("Manual grouping"), (gpointer)&tb->manual_grouping, (GType)CONF_TYPE_BOOL,
        "", 0, (GType)CONF_TYPE_END_TABLE,

        _("Show iconified windows"), (gpointer)&tb->show_iconified, (GType)CONF_TYPE_BOOL,
        _("Show mapped windows"), (gpointer)&tb->show_mapped, (GType)CONF_TYPE_BOOL,
        _("Show windows from all desktops"), (gpointer)&tb->show_all_desks, (GType)CONF_TYPE_BOOL,

        _("Flash when there is any window requiring attention"), (gpointer)&tb->use_urgency_hint, (GType)CONF_TYPE_BOOL,

        _("Bindings"), (gpointer)NULL, (GType)CONF_TYPE_BEGIN_PAGE,

        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        button1_action, (gpointer)&tb->button1_action, (GType)CONF_TYPE_ENUM,
        button2_action, (gpointer)&tb->button2_action, (GType)CONF_TYPE_ENUM,
        button3_action, (gpointer)&tb->button3_action, (GType)CONF_TYPE_ENUM,
        scroll_up_action, (gpointer)&tb->scroll_up_action, (GType)CONF_TYPE_ENUM,
        scroll_down_action, (gpointer)&tb->scroll_down_action, (GType)CONF_TYPE_ENUM,
        shift_button1_action, (gpointer)&tb->shift_button1_action, (GType)CONF_TYPE_ENUM,
        shift_button2_action, (gpointer)&tb->shift_button2_action, (GType)CONF_TYPE_ENUM,
        shift_button3_action, (gpointer)&tb->shift_button3_action, (GType)CONF_TYPE_ENUM,
        shift_scroll_up_action, (gpointer)&tb->shift_scroll_up_action, (GType)CONF_TYPE_ENUM,
        shift_scroll_down_action, (gpointer)&tb->shift_scroll_down_action, (GType)CONF_TYPE_ENUM,
        "", 0, (GType)CONF_TYPE_END_TABLE,

        NULL);

    if (dlg)
        gtk_window_present(GTK_WINDOW(dlg));

    g_free(button1_action);
    g_free(button2_action);
    g_free(button3_action);
    g_free(scroll_up_action);
    g_free(scroll_down_action);
    g_free(shift_button1_action);
    g_free(shift_button2_action);
    g_free(shift_button3_action);
    g_free(shift_scroll_up_action);
    g_free(shift_scroll_down_action);
}

/* Save the configuration to the configuration file. */
static void taskbar_save_configuration(Plugin * p, FILE * fp)
{
    TaskbarPlugin * tb = (TaskbarPlugin *) p->priv;
    lxpanel_put_bool(fp, "tooltips", tb->tooltips);
    lxpanel_put_enum(fp, "ShowIconsTitles", tb->show_icons_titles, show_pair);
    lxpanel_put_bool(fp, "ShowIconified", tb->show_iconified);
    lxpanel_put_bool(fp, "ShowMapped", tb->show_mapped);
    lxpanel_put_bool(fp, "ShowAllDesks", tb->show_all_desks);
    lxpanel_put_bool(fp, "UseUrgencyHint", tb->use_urgency_hint);
    lxpanel_put_bool(fp, "FlatButton", tb->flat_button);
    lxpanel_put_int(fp, "MaxTaskWidth", tb->task_width_max);
    lxpanel_put_int(fp, "spacing", tb->spacing);
    lxpanel_put_enum(fp, "Mode", tb->mode, mode_pair);
    lxpanel_put_int(fp, "GroupShrinkThreshold", tb->group_shrink_threshold);
    lxpanel_put_enum(fp, "GroupBy", tb->group_by, group_by_pair);
    lxpanel_put_bool(fp, "ManualGrouping", tb->manual_grouping);
    lxpanel_put_bool(fp, "ExpandFocusedGroup", tb->expand_focused_group);
    lxpanel_put_bool(fp, "ShowCloseButtons", tb->show_close_buttons);
    lxpanel_put_enum(fp, "Button1Action", tb->button1_action, action_pair);
    lxpanel_put_enum(fp, "Button2Action", tb->button2_action, action_pair);
    lxpanel_put_enum(fp, "Button3Action", tb->button3_action, action_pair);
    lxpanel_put_enum(fp, "ScrollUpAction", tb->scroll_up_action, action_pair);
    lxpanel_put_enum(fp, "ScrollDownAction", tb->scroll_down_action, action_pair);
    lxpanel_put_enum(fp, "ShiftButton1Action", tb->shift_button1_action, action_pair);
    lxpanel_put_enum(fp, "ShiftButton2Action", tb->shift_button2_action, action_pair);
    lxpanel_put_enum(fp, "ShiftButton3Action", tb->shift_button3_action, action_pair);
    lxpanel_put_enum(fp, "ShiftScrollUpAction", tb->shift_scroll_up_action, action_pair);
    lxpanel_put_enum(fp, "ShiftScrollDownAction", tb->shift_scroll_down_action, action_pair);
}

/* Callback when panel configuration changes. */
static void taskbar_panel_configuration_changed(Plugin * p)
{
    TaskbarPlugin * tb = (TaskbarPlugin *) p->priv;
    taskbar_update_style(tb);
    taskbar_make_menu(tb);
    GtkOrientation bo = (tb->plug->panel->orientation == ORIENT_HORIZ) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    icon_grid_set_expand(tb->icon_grid, get_task_button_expandable(tb));
    icon_grid_set_geometry(tb->icon_grid, bo,
        get_task_button_max_width(tb), tb->plug->panel->icon_size + BUTTON_HEIGHT_EXTRA,
        tb->spacing, 0, tb->plug->panel->height);

    /* If the icon size changed, refetch all the icons. */
    if (tb->plug->panel->icon_size != tb->icon_size)
    {
        tb->icon_size = tb->plug->panel->icon_size;
        Task * tk;
        for (tk = tb->task_list; tk != NULL; tk = tk->task_flink)
        {
            task_update_icon(tk, None);
        }
    }

    /* Redraw all the labels.  Icon size or font color may have changed. */
    taskbar_redraw(tb);
}

/* Plugin descriptor. */
PluginClass taskbar_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "taskbar",
    name : N_("Task Bar (Window List)"),
    version: "1.0",
    description : N_("Taskbar shows all opened windows and allow to iconify them, shade or get focus"),

    /* Stretch is available and default for this plugin. */
    expand_available : TRUE,
    expand_default : TRUE,

    constructor : taskbar_constructor,
    destructor  : taskbar_destructor,
    config : taskbar_configure,
    save : taskbar_save_configuration,
    panel_configuration_changed : taskbar_panel_configuration_changed

};
