/**
 * Copyright (c) 2011-2012 Vadim Ushakov
 * Copyright (c) 2006 LxDE Developers
 * Copyright (c) 2006 Hong Jen Yee (PCMan)
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


/********************\
 = HC SVNT DRACONES =
\********************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

#define PLUGIN_PRIV_TYPE TaskbarPlugin

#include <lxpanelx/gtkcompat.h>
#include <lxpanelx/global.h>
#include <lxpanelx/panel.h>
#include <lxpanelx/misc.h>
#include <lxpanelx/plugin.h>
#include <lxpanelx/Xsupport.h>
#include "icon.xpm"
#include <lxpanelx/icon-grid.h>

//#define DEBUG

#include <lxpanelx/dbg.h>

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
    ACTION_PREV_WINDOW_IN_GROUP,
    ACTION_COPY_TITLE
};

enum MOUSE_OVER_ACTION {
    MOUSE_OVER_ACTION_NONE,
    MOUSE_OVER_ACTION_GROUP_MENU,
    MOUSE_OVER_ACTION_PREVIEW
};


enum {
    TRIGGERED_BY_CLICK,
    TRIGGERED_BY_PRESS
};

enum {
    GROUP_BY_NONE,
    GROUP_BY_CLASS,
    GROUP_BY_WORKSPACE,
    GROUP_BY_STATE
};

enum {
    SORT_BY_TIMESTAMP,
    SORT_BY_TITLE,
    SORT_BY_FOCUS,
    SORT_BY_STATE,
    SORT_BY_WORKSPACE
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
    { ACTION_COPY_TITLE, "CopyTitle"},
    { 0, NULL},
};

static pair mouse_over_action_pair[] = {
    { MOUSE_OVER_ACTION_NONE, "None"},
    { MOUSE_OVER_ACTION_GROUP_MENU, "ShowGroupedWindowList"},
    { MOUSE_OVER_ACTION_PREVIEW, "ShowPreview"},
};

static pair action_trigged_by_pair[] = {
    { TRIGGERED_BY_CLICK, "Click"},
    { TRIGGERED_BY_PRESS, "Press"},
    { 0, NULL},
};

static pair group_by_pair[] = {
    { GROUP_BY_NONE, "None" },
    { GROUP_BY_CLASS, "Class" },
    { GROUP_BY_WORKSPACE,  "Workspace" },
    { GROUP_BY_STATE,  "State" },
    { 0, NULL},
};

static pair sort_by_pair[] = {
    { SORT_BY_TIMESTAMP, "Timestamp" },
    { SORT_BY_TITLE, "Title" },
    { SORT_BY_FOCUS, "Focus" },
    { SORT_BY_STATE, "State" },
    { SORT_BY_WORKSPACE,  "Workspace" },
    { 0, NULL},
};

/******************************************************************************/

struct _taskbar;
struct _task_class;
struct _task;

/* Structure representing a class. */
typedef struct _task_class {
    struct _task_class * task_class_flink;	/* Forward link */
    char * class_name;				/* Class name */
    struct _task * task_class_head;		/* Head of list of tasks with this class */
    struct _task * visible_task;		/* Task that is visible in current desktop, if any */
    char * visible_name;			/* Name that will be visible for grouped tasks */
    int visible_count;				/* Count of tasks that are visible in current desktop */
    int timestamp;

    int manual_order;

    gboolean fold_by_count;

    gboolean unfold;
    gboolean manual_unfold_state;
} TaskClass;

/* Structure representing a "task", an open window. */
typedef struct _task {

    struct _task * task_flink;			/* Forward link to next task in X window ID order */
    struct _taskbar * tb;			/* Back pointer to taskbar */
    Window win;					/* X window ID */


    char * name;				/* Taskbar label when normal, from WM_NAME or NET_WM_NAME */
    char * name_iconified;			/* Taskbar label when iconified */
    char * name_shaded;				/* Taskbar label when shaded */
    Atom name_source;				/* Atom that is the source of taskbar label */
    gboolean name_changed;

    char * wm_class;

    TaskClass * task_class;			/* Task class (group) */
    struct _task * task_class_flink;		/* Forward link to task in same class */
    char * override_class_name;

    GtkWidget * button;				/* Button representing task in taskbar */
    GtkWidget * container;			/* Container for image, label and close button. */
    GtkWidget * image;				/* Icon for task, child of button */
    Atom image_source;				/* Atom that is the source of taskbar icon */
    GtkWidget * label;				/* Label for task, child of button */
    GtkWidget * button_close;			/* Close button */

    GtkWidget * new_group_dlg;			/* Move to new group dialog */


    GtkAllocation button_alloc;
    guint adapt_to_allocated_size_idle_cb;
    guint update_icon_idle_cb;


    int desktop;				/* Desktop that contains task, needed to switch to it on Raise */
    guint flash_timeout;			/* Timer for urgency notification */
    unsigned int focused : 1;			/* True if window has focus */
    unsigned int iconified : 1;			/* True if window is iconified, from WM_STATE */
    unsigned int maximized : 1;			/* True if window is maximized, from WM_STATE */
    unsigned int decorated : 1;			/* True if window is decorated, from _MOTIF_WM_HINTS or _OB_WM_STATE_UNDECORATED */
    unsigned int shaded : 1;			/* True if window is shaded, from WM_STATE */
    unsigned int urgency : 1;			/* True if window has an urgency hint, from WM_HINTS */
    unsigned int flash_state : 1;		/* One-bit counter to flash taskbar */
    unsigned int entered_state : 1;		/* True if cursor is inside taskbar button */
    unsigned int present_in_client_list : 1;	/* State during WM_CLIENT_LIST processing to detect deletions */

    unsigned int deferred_iconified_update : 1;

    int timestamp;

    int focus_timestamp;

    int manual_order;

    GtkWidget* click_on;

    gboolean separator;

    int x_window_position;

    guint show_popup_delay_timer;

    /* Icons, thumbnails. */

    int allocated_icon_size;           /* available room for the icon in the container */
    int icon_size;                     /* real size of currently used icon (icon_pixbuf, icon_pixbuf_iconified, thumbnail_icon) */
    gboolean forse_icon_erase;

    GdkPixbuf * icon_pixbuf;           /* Resulting icon image for visible windows */
    GdkPixbuf * icon_pixbuf_iconified; /* Resulting icon image for iconified windows. */

    GdkPixbuf * icon_for_bgcolor;
    guint update_bgcolor_cb;

    Pixmap backing_pixmap;             /* Backing pixmap of the window. (0 if not visible) */
    GdkPixbuf * thumbnail;             /* Latest copy of window content (full size). If backing_pixmap became 0, thumbnail stays valid.*/
    GdkPixbuf * thumbnail_icon;        /* thumbnail, scaled to icon_size */
    GdkPixbuf * thumbnail_preview;     /* thumbnail, scaled to preview size */
    guint update_composite_thumbnail_timeout; /* update_composite_thumbnail event source id */
    guint update_composite_thumbnail_idle;
    gboolean require_update_composite_thumbnail;
    int update_composite_thumbnail_repeat_count;
    guint update_thumbnail_preview_idle; /* update_thumbnail_preview event source id */

    GtkWidget * preview_image;          /* image on preview panel */

    /* Background colors from icon */
    GdkColor bgcolor1; /* normal */
    GdkColor bgcolor2; /* prelight */

    /*  */

    gchar * run_path;

} Task;

/* Private context for taskbar plugin. */
typedef struct _taskbar {

    Plugin * plug;				/* Back pointer to Plugin */
    Task * task_list;				/* List of tasks to be displayed in taskbar */
    TaskClass * task_class_list;		/* Window class list */
    IconGrid * icon_grid;			/* Manager for taskbar buttons */

    int task_timestamp;                         /* To sort tasks and task classes by creation time. */

    GdkPixbuf * custom_fallback_pixbuf;		/* Custom fallback task icon when none is available */
    GdkPixbuf * fallback_pixbuf;		/* Default fallback task icon when none is available */

    /* Geometry */

    int icon_size;				/* Size of task icons (from panel settings) */
    int expected_icon_size;			/* Expected icon size (from task button allocation data) */
    int extra_size;

    /* Context menu */

    gchar ** menu_config;
    GtkWidget * menu;				/* Popup menu for task control (Close, Raise, etc.) */
    GtkWidget * workspace_submenu;		/* Workspace submenu of the task control menu */
    GtkWidget * move_to_this_workspace_menuitem;
    GtkWidget * restore_menuitem;
    GtkWidget * maximize_menuitem;
    GtkWidget * iconify_menuitem;
    GtkWidget * roll_menuitem;
    GtkWidget * undecorate_menuitem;
    GtkWidget * ungroup_menuitem;
    GtkWidget * move_to_group_menuitem;
    GtkWidget * unfold_group_menuitem;
    GtkWidget * fold_group_menuitem;
    GtkWidget * title_menuitem;
    GtkWidget * run_new_menuitem;

    /* Task popup: group menu or preview panel. */

    GtkWidget * group_menu;			/* Group menu */
    GtkAllocation group_menu_alloc;
    gboolean group_menu_opened_as_popup;

    GtkWidget * preview_panel_window;
    GtkWidget * preview_panel_box;
    GtkAllocation preview_panel_window_alloc;
    guint preview_panel_motion_timer;
    int preview_panel_speed;
    int preview_panel_mouse_position;

    Task * popup_task;                          /* Task that owns popup. */
    guint hide_popup_delay_timer;               /* Timer to close popup if mouse leaves it */


    /* NETWM stuff */

    gboolean use_net_active;			/* NET_WM_ACTIVE_WINDOW is supported by the window manager */
    gboolean net_active_checked;		/* True if use_net_active is valid */

    char * * desktop_names;
    int number_of_desktop_names;
    int number_of_desktops;			/* Number of desktops, from NET_WM_NUMBER_OF_DESKTOPS */
    int current_desktop;			/* Current desktop, from NET_WM_CURRENT_DESKTOP */
    Task * focused;				/* Task that has focus */
    Task * focused_previous;			/* Task that had focus just before panel got it */
    Task * menutask;				/* Task for which popup menu is open */

    guint dnd_delay_timer;			/* Timer for drag and drop delay */

    /* User preferences */

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

    int menu_actions_click_press;
    int other_actions_click_press;

    int mouse_over_action;

    gboolean show_all_desks;			/* User preference: show windows from all desktops */
    gboolean show_mapped;			/* User preference: show mapped windows */
    gboolean show_iconified;			/* User preference: show iconified windows */
    gboolean tooltips;				/* User preference: show tooltips */
    int show_icons_titles;			/* User preference: show icons, titles */

    char* custom_fallback_icon; /* User preference: use as fallback icon */

    gboolean show_close_buttons;		/* User preference: show close button */

    gboolean show_urgency_all_desks;		/* User preference: show windows from other workspaces if they set urgent hint*/
    gboolean use_urgency_hint;			/* User preference: windows with urgency will flash */
    gboolean flat_inactive_buttons;
    gboolean flat_active_button;

    gboolean bold_font_on_mouse_over;

    gboolean use_group_separators;
    int group_separator_size;

    gboolean dimm_iconified;

    gboolean colorize_buttons;

    gboolean use_thumbnails_as_icons;

    int mode;                                   /* User preference: view mode */
    int group_fold_threshold;                   /* User preference: threshold for fold grouped tasks into one button */
    int panel_fold_threshold;
    int group_by;                               /* User preference: attr to group tasks by */
    gboolean manual_grouping;			/* User preference: manual grouping */
    gboolean unfold_focused_group;		/* User preference: autounfold group of focused window */
    gboolean show_single_group;			/* User preference: show windows of the active group only  */

    int sort_by[3];
    gboolean sort_reverse[3];

    gboolean rearrange;

    gboolean highlight_modified_titles;		/* User preference: highlight modified titles */

    int task_width_max;				/* Maximum width of a taskbar button in horizontal orientation */
    int spacing;				/* Spacing between taskbar buttons */

    gboolean use_x_net_wm_icon_geometry;
    gboolean use_x_window_position;


    /* Effective config values, evaluated from "User preference" variables: */
    gboolean grouped_tasks;			/* Group task of the same class into single button. */
    gboolean single_window;			/* Show only current window button. */
    gboolean rebuild_gui;			/* Force gui rebuild (when configuration changed) */
    gboolean show_all_desks_prev_value;         /* Value of show_all_desks from last gui rebuild */
    gboolean show_icons;			/* Show icons */
    gboolean show_titles;			/* Show title labels */
    gboolean _show_close_buttons;               /* Show close buttons */
    int _group_fold_threshold;
    int _panel_fold_threshold;
    int _group_by;
    int _mode;
    gboolean _unfold_focused_group;
    gboolean _show_single_group;

    gboolean show_mapped_prev;
    gboolean show_iconified_prev;

    gboolean dimm_iconified_prev;
    gboolean colorize_buttons_prev;
    gboolean use_group_separators_prev;

    int sort_settings_hash;

    /* Deferred window switching data. */

    guint deferred_desktop_switch_timer;
    int deferred_current_desktop;
    int deferred_active_window_valid;
    Window deferred_active_window;

    gboolean open_group_menu_on_mouse_over;
    gboolean thumbnails_preview;
    gboolean thumbnails;

    Task * button_pressed_task;
    gboolean moving_task_now;

    GdkColormap * color_map; /* cached value of panel_get_color_map(plug->panel) */
} TaskbarPlugin;

/******************************************************************************/

static Atom atom_LXPANEL_TASKBAR_WINDOW_POSITION;

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
#define OPEN_GROUP_MENU_DELAY 500
#define TASK_WIDTH_MAX       200
#define TASK_PADDING         4
#define ALL_WORKSPACES       0xFFFFFFFF		/* 64-bit clean */
#define ICON_ONLY_EXTRA      6		/* Amount needed to have button lay out symmetrically */
#define BUTTON_HEIGHT_EXTRA  4          /* Amount needed to have button not clip icon */

static void set_timer_on_task(Task * tk);

static gboolean task_is_visible_on_current_desktop(Task * tk);
static gboolean task_is_visible_on_desktop(Task * tk, int desktop);

static gboolean task_is_visible(Task * tk);

static void recompute_group_visibility_for_class(TaskbarPlugin * tb, TaskClass * tc);
static void recompute_group_visibility_on_current_desktop(TaskbarPlugin * tb);

static void taskbar_update_separators(TaskbarPlugin * tb);

static void task_draw_label(Task * tk);
static void task_button_redraw(Task * tk);
static void taskbar_redraw(TaskbarPlugin * tb);

static gboolean accept_net_wm_state(NetWMState * nws);
static gboolean accept_net_wm_window_type(NetWMWindowType * nwwt);
static void task_free_names(Task * tk);
static void task_set_names(Task * tk, Atom source);

static void task_unlink_class(Task * tk);
static TaskClass * taskbar_enter_class(TaskbarPlugin * tb, char * class_name, gboolean * name_consumed);
static void task_set_class(Task * tk);

static Task * task_lookup(TaskbarPlugin * tb, Window win);
static void task_delete(TaskbarPlugin * tb, Task * tk, gboolean unlink);
static void task_update_icon(Task * tk, Atom source, gboolean forse_icon_erase);
static void task_defer_update_icon(Task * tk, gboolean forse_icon_erase);

static void task_reorder(Task * tk, gboolean and_others);
static void task_update_grouping(Task * tk, int group_by);
static void task_update_sorting(Task * tk, int sort_by);

static void taskbar_check_hide_popup(TaskbarPlugin * tb);
static void taskbar_hide_popup(TaskbarPlugin * tb);

static gboolean flash_window_timeout(Task * tk);
static void task_set_urgency(Task * tk);
static void task_clear_urgency(Task * tk);
static void task_raise_window(Task * tk, guint32 time);
static void taskbar_popup_set_position(GtkWidget * menu, gint * px, gint * py, gboolean * push_in, gpointer data);
static void taskbar_group_menu_destroy(TaskbarPlugin * tb);
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

/******************************************************************************/

/* Taskbar internal options and properties. */

static int taskbar_get_task_button_max_width(TaskbarPlugin * tb)
{
    int icon_mode_max_width = tb->icon_size + ICON_ONLY_EXTRA + (tb->_show_close_buttons ? tb->extra_size : 0);
    if (tb->show_titles && tb->task_width_max > icon_mode_max_width) {
        return tb->task_width_max;
    } else {
        return icon_mode_max_width;
    }
}

static gboolean taskbar_task_button_is_expandable(TaskbarPlugin * tb) {
        return tb->single_window || tb->task_width_max < 1;
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

static gboolean taskbar_has_visible_tasks_on_desktop(TaskbarPlugin * tb, int desktop)
{
    Task * tk;
    for (tk = tb->task_list; tk != NULL; tk = tk->task_flink)
        if (task_is_visible_on_desktop(tk,  desktop))
            return TRUE;
    return FALSE;
}

/******************************************************************************/

/* Task class getters. */

static int task_class_is_folded(TaskbarPlugin * tb, TaskClass * tc)
{
    if (!tb->grouped_tasks)
        return FALSE;

    if (tc && tc->manual_unfold_state)
        return !tc->unfold;

    if ((tb->_unfold_focused_group || tb->_show_single_group) && tb->focused && tb->focused->task_class == tc)
        return FALSE;

    if (tc && tc->fold_by_count)
        return TRUE;

    int visible_count = tc ? tc->visible_count : 1;
    return (tb->_group_fold_threshold > 0) && (visible_count >= tb->_group_fold_threshold);
}

/******************************************************************************/

/* Task getters. */

static char* task_get_displayed_name(Task * tk)
{
    if (tk->shaded) {
        if (!tk->name_shaded) {
            tk->name_shaded = g_strdup_printf("=%s=", tk->name);
        }
        return tk->name_shaded;
    }
    else if (tk->iconified) {
        if (!tk->name_iconified) {
            tk->name_iconified = g_strdup_printf("[%s]", tk->name);
        }
        return tk->name_iconified;
    } else {
        return tk->name;
    }
}

static gchar* task_get_desktop_name(Task * tk, const char* defval)
{
    return taskbar_get_desktop_name(tk->tb, tk->desktop, defval);
}

static int task_is_folded(Task * tk)
{
    return task_class_is_folded(tk->tb, tk->task_class);
}

static gboolean task_has_visible_close_button(Task * tk)
{
    return tk->tb->_show_close_buttons && !task_is_folded(tk);
}

/* Determine if a task is visible considering only its desktop placement. */
static gboolean task_is_visible_on_desktop(Task * tk, int desktop)
{
    return ( (tk->desktop == ALL_WORKSPACES)
          || (tk->desktop == desktop)
          || (tk->tb->show_all_desks)
          || (tk->tb->show_urgency_all_desks && tk->urgency) );
}

/* Determine if a task is visible considering only its desktop placement. */
static gboolean task_is_visible_on_current_desktop(Task * tk)
{
    return task_is_visible_on_desktop(tk, tk->tb->current_desktop);
}

/* Determine if a task is visible. */
static gboolean task_is_visible(Task * tk)
{
    TaskbarPlugin * tb = tk->tb;

    /* Not visible due to grouping. */
    if (task_is_folded(tk) && (tk->task_class) && (tk->task_class->visible_task != tk))
        return FALSE;

    /* In single_window mode only focused task is visible. */
    if (tb->single_window && !tk->focused)
        return FALSE;

    if (tb->_show_single_group && !tk->focused && (!tk->task_class || !tb->focused || tb->focused->task_class != tk->task_class))
        return FALSE;

    /* Hide iconified or mapped tasks? */
    if (!tb->single_window && !((tk->iconified && tb->show_iconified) || (!tk->iconified && tb->show_mapped)) )
        return FALSE;

    /* Desktop placement. */
    return task_is_visible_on_current_desktop(tk);
}

static int task_button_is_really_flat(Task * tk)
{
    TaskbarPlugin * tb = tk->tb;
    return ( tb->single_window ) ||
		(tb->flat_inactive_buttons && !tk->focused ) ||
		(tb->flat_active_button && tk->focused );
}

/******************************************************************************/

static void taskbar_recompute_fold_by_count(TaskbarPlugin * tb)
{
    if (tb->_panel_fold_threshold < 1)
        return;

    if (!tb->grouped_tasks)
        return;

    ENTER;

    TaskClass * tc;

    for (tc = tb->task_class_list; tc != NULL; tc = tc->task_class_flink)
    {
        tc->fold_by_count = FALSE;
    }

    int total_visible_count;
    int max_visible_count;

    do
    {
        total_visible_count = 0;
        max_visible_count = 1;

        TaskClass *  max_tc = NULL;

        for (tc = tb->task_class_list; tc != NULL; tc = tc->task_class_flink)
        {
            int visible_count = tc->visible_count;
            if (visible_count > 1 && task_class_is_folded(tb, tc))
                visible_count = 1;

            total_visible_count += visible_count;
            if (visible_count > max_visible_count && !tc->fold_by_count)
            {
                max_tc = tc;
                max_visible_count = visible_count;
            }
        }

        if (total_visible_count > tb->_panel_fold_threshold && max_visible_count > 1)
        {
            total_visible_count -= max_visible_count - 1;
            max_tc->fold_by_count = TRUE;
            recompute_group_visibility_for_class(tb, max_tc);
        }
    } while (total_visible_count > tb->_panel_fold_threshold && max_visible_count > 1) ;

    RET();
}

/******************************************************************************/

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

    for (tk = tc->task_class_head; tk != NULL; tk = tk->task_class_flink)
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
            else if ((tc->visible_name != tc->class_name)
            && (tc->visible_name != NULL) && (tk->name != NULL)
            && (strcmp(tc->visible_name, tk->name) != 0))
                tc->visible_name = tc->class_name;
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
    for (tc = tb->task_class_list; tc != NULL; tc = tc->task_class_flink)
    {
        recompute_group_visibility_for_class(tb, tc);
    }
}

/******************************************************************************/

static void taskbar_update_x_window_position(TaskbarPlugin * tb)
{
    if (!tb->use_x_window_position)
        return;

    Task * tk;
    int position = 0;
    for (tk = tb->task_list; tk != NULL; tk = tk->task_flink)
    {
        int p = -1;
        if (task_is_visible(tk))
            p = position++;
        if (p != tk->x_window_position)
        {
            tk->x_window_position = p;
            gint32 data = p;
            XChangeProperty(gdk_x11_get_default_xdisplay(), tk->win,
                atom_LXPANEL_TASKBAR_WINDOW_POSITION,
                XA_CARDINAL, 32, PropModeReplace, (guchar *) &data, 1);
        }
    }
}

/******************************************************************************/

/* Draw the label and tooltip on a taskbar button. */
static void task_draw_label(Task * tk)
{
    TaskClass * tc = tk->task_class;

    gboolean bold_style = tk->entered_state && tk->tb->bold_font_on_mouse_over;
	bold_style |= tk->flash_state && task_button_is_really_flat(tk);
    bold_style |= tk->name_changed && tk->tb->highlight_modified_titles;

    if (task_is_folded(tk) && (tc) && (tc->visible_task == tk))
    {
        char * label = g_strdup_printf("(%d) %s", tc->visible_count, tc->visible_name);
        gtk_widget_set_tooltip_text(tk->button, label);
        if (tk->label)
            panel_draw_label_text(plugin_panel(tk->tb->plug), tk->label, label, bold_style, task_button_is_really_flat(tk));
        g_free(label);
    }
    else
    {
        char * name = task_get_displayed_name(tk);
        if (tk->tb->tooltips)
            gtk_widget_set_tooltip_text(tk->button, name);
        if (tk->label)
            panel_draw_label_text(plugin_panel(tk->tb->plug), tk->label, name, bold_style, task_button_is_really_flat(tk));
    }
}

static void task_button_redraw_button_state(Task * tk, TaskbarPlugin * tb)
{
    if (task_button_is_really_flat(tk))
    {
        gtk_toggle_button_set_active((GtkToggleButton*)tk->button, FALSE);
        gtk_button_set_relief(GTK_BUTTON(tk->button), GTK_RELIEF_NONE);
    }
    else
    {
        gboolean pressed = tk->focused /*|| (tb->button_pressed_task == tk)*/;
        gtk_toggle_button_set_active((GtkToggleButton*)tk->button, pressed);
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

    ENTER;

    if (task_is_visible(tk))
    {
        task_button_redraw_button_state(tk, tb);
        if (tk->deferred_iconified_update)
        {
            tk->deferred_iconified_update = FALSE;
            task_update_icon(tk, None, FALSE);
        }
        task_draw_label(tk);
        icon_grid_set_visible(tb->icon_grid, tk->button, TRUE);
    }
    else
    {
        icon_grid_set_visible(tb->icon_grid, tk->button, FALSE);
    }

    RET();
}

/* Redraw all tasks in the taskbar. */
static void taskbar_redraw(TaskbarPlugin * tb)
{
    if (!tb->icon_grid)
        return;

    ENTER;

    icon_grid_defer_updates(tb->icon_grid);

    taskbar_recompute_fold_by_count(tb);

    Task * tk;
    for (tk = tb->task_list; tk != NULL; tk = tk->task_flink)
        task_button_redraw(tk);

    taskbar_update_separators(tb);

    icon_grid_resume_updates(tb->icon_grid);

    taskbar_update_x_window_position(tb);

    RET();
}

/******************************************************************************/

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

/******************************************************************************/

/* Free the names associated with a task. */
static void task_free_names(Task * tk)
{
    g_free(tk->name);
    g_free(tk->name_iconified);
    g_free(tk->name_shaded);
    tk->name = tk->name_iconified = tk->name_shaded = NULL;
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
    if (name != NULL && (!tk->name || strcmp(name, tk->name) != 0))
    {
        task_free_names(tk);
        tk->name = g_strdup(name);
        g_free(name);

        tk->name_changed = !tk->focused;

        /* Update tk->task_class->visible_name as it may point to freed tk->name. */
        if (tk->task_class && tk->tb)
        {
            recompute_group_visibility_for_class(tk->tb, tk->task_class);
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
    TaskClass * tc = tk->task_class;
    if (tc != NULL)
    {
        tk->task_class = NULL;

        if (tc->visible_task == tk)
            tc->visible_task = NULL;

        /* Remove from per-class task list. */
        if (tc->task_class_head == tk)
        {
            /* Removing the head of the list.  This causes a new task to be the visible task, so we redraw. */
            tc->task_class_head = tk->task_class_flink;
            tk->task_class_flink = NULL;
            if (tc->task_class_head != NULL)
                task_button_redraw(tc->task_class_head);
        }
        else
        {
            /* Locate the task and its predecessor in the list and then remove it.  For safety, ensure it is found. */
            Task * tk_pred = NULL;
            Task * tk_cursor;
            for (
              tk_cursor = tc->task_class_head;
              ((tk_cursor != NULL) && (tk_cursor != tk));
              tk_pred = tk_cursor, tk_cursor = tk_cursor->task_class_flink) ;
            if (tk_cursor == tk)
                tk_pred->task_class_flink = tk->task_class_flink;
            tk->task_class_flink = NULL;
        }

        /* Recompute group visibility. */
        recompute_group_visibility_for_class(tk->tb, tc);
    }
    RET();
}

/* Enter class with specified name. */
static TaskClass * taskbar_enter_class(TaskbarPlugin * tb, char * class_name, gboolean * name_consumed)
{
    ENTER;
    /* Find existing entry or insertion point. */
    *name_consumed = FALSE;
    TaskClass * tc_pred = NULL;
    TaskClass * tc;
    for (tc = tb->task_class_list; tc != NULL; tc_pred = tc, tc = tc->task_class_flink)
    {
        int status = strcmp(class_name, tc->class_name);
        if (status == 0)
            RET(tc);
        if (status < 0)
            break;
    }

    /* Insert new entry. */
    tc = g_new0(TaskClass, 1);
    tc->class_name = class_name;
    *name_consumed = TRUE;
    if (tc_pred == NULL)
    {
        tc->task_class_flink = tb->task_class_list;
        tb->task_class_list = tc;
    }
    else
    {
        tc->task_class_flink = tc_pred->task_class_flink;
	tc_pred->task_class_flink = tc;
    }
    RET(tc);
}

static gchar* task_read_wm_class(Task * tk) {
    ENTER;
    /* Read the WM_CLASS property. */
    XClassHint ch;
    ch.res_name = NULL;
    ch.res_class = NULL;
    XGetClassHint(gdk_x11_get_default_xdisplay(), tk->win, &ch);

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

static void task_update_wm_class(Task * tk) {
    g_free(tk->wm_class);
    tk->wm_class = task_read_wm_class(tk);

    if (tk->run_path && tk->run_path != (gchar *)-1)
        g_free(tk->run_path);
    tk->run_path = (gchar *)-1;
}

/* Set the class associated with a task. */
static void task_set_class(Task * tk)
{
    ENTER;

    g_assert(tk != NULL);

    gchar * class_name = NULL;

    TaskbarPlugin * tb = tk->tb;

    if (tb->rearrange) {
        /* FIXME: Dirty hack for current implementation of task_reorder. */
        gchar * class_name_1 = tk->wm_class;
        gchar * class_name_2 = task_get_desktop_name(tk, NULL);
        class_name = g_strdup_printf("%s %s", class_name_1, class_name_2);
        g_free(class_name_2);
    } else if (tk->override_class_name != (char*) -1) {
        class_name = task_get_desktop_name(tk, NULL);
    } else {
        switch (tb->_group_by) {
            case GROUP_BY_CLASS:
                class_name = g_strdup(tk->wm_class); break;
            case GROUP_BY_WORKSPACE:
                class_name = task_get_desktop_name(tk, NULL); break;
            case GROUP_BY_STATE:
                class_name = g_strdup(
                    (tk->urgency) ? _("Urgency") :
                    (tk->iconified) ? _("Iconified") :
                    _("Mapped")
                );
                break;
        }
    }

    if (class_name != NULL)
    {
        DBG("Task %s has class name %s\n", tk->name, class_name);

        gboolean name_consumed;
        TaskClass * tc = taskbar_enter_class(tb, class_name, &name_consumed);
        if ( ! name_consumed)
            g_free(class_name);

        /* If the task changed class, update data structures. */
        TaskClass * old_tc = tk->task_class;
        if (old_tc != tc)
        {
            /* Unlink from previous class, if any. */
            if (old_tc)
                task_unlink_class(tk);

            if (!tc->timestamp)
                tc->timestamp = tk->timestamp;

            /* Add to end of per-class task list.  Do this to keep the popup menu in order of creation. */
            if (tc->task_class_head == NULL)
                tc->task_class_head = tk;
            else
            {
                Task * tk_pred;
                for (tk_pred = tc->task_class_head; tk_pred->task_class_flink != NULL; tk_pred = tk_pred->task_class_flink) ;
                tk_pred->task_class_flink = tk;
                g_assert(tk->task_class_flink == NULL);
                task_button_redraw(tk);
            }
            tk->task_class = tc;

            /* Recompute group visibility. */
            recompute_group_visibility_for_class(tb, tc);
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

    if (tk->run_path && tk->run_path != (gchar *)-1)
        g_free(tk->run_path);

    if (tk->preview_image)
        g_object_unref(G_OBJECT(tk->preview_image));

    if (tk == tb->popup_task ||
        (tb->popup_task && tb->popup_task->task_class && tb->popup_task->task_class == tk->task_class))
    {
        taskbar_hide_popup(tk->tb);
    }

    if (tk->bgcolor1.pixel)
        gdk_colormap_free_colors(tb->color_map, &tk->bgcolor1, 1);
    if (tk->bgcolor2.pixel)
        gdk_colormap_free_colors(tb->color_map, &tk->bgcolor2, 1);
    if (tk->icon_for_bgcolor)
        g_object_unref(G_OBJECT(tk->icon_for_bgcolor));
    if (tk->update_bgcolor_cb)
        g_source_remove(tk->update_bgcolor_cb);


    /* Free thumbnails. */
    if (tk->backing_pixmap != 0)
        XFreePixmap(gdk_x11_get_default_xdisplay(), tk->backing_pixmap);
    if (tk->thumbnail)
        g_object_unref(G_OBJECT(tk->thumbnail));
    if (tk->thumbnail_icon)
        g_object_unref(G_OBJECT(tk->thumbnail_icon));
    if (tk->thumbnail_preview)
        g_object_unref(G_OBJECT(tk->thumbnail_preview));
    if (tk->update_composite_thumbnail_timeout)
        g_source_remove(tk->update_composite_thumbnail_timeout);
    if (tk->update_thumbnail_preview_idle)
        g_source_remove(tk->update_thumbnail_preview_idle);

    /* If we think this task had focus, remove that. */
    if (tb->focused == tk)
        tb->focused = NULL;

    if (tb->button_pressed_task == tk)
        tb->button_pressed_task = NULL;

    /* Remove deferred calls and timers. */
    if (tk->adapt_to_allocated_size_idle_cb != 0)
        g_source_remove(tk->adapt_to_allocated_size_idle_cb);
    if (tk->update_icon_idle_cb != 0)
        g_source_remove(tk->update_icon_idle_cb);
    if (tk->show_popup_delay_timer != 0)
        g_source_remove(tk->show_popup_delay_timer);

    if (tk->override_class_name != (char*) -1 && tk->override_class_name)
         g_free(tk->override_class_name);

    if (tk->new_group_dlg)
        gtk_widget_destroy(tk->new_group_dlg);

    /* If there is an urgency timeout, remove it. */
    if (tk->flash_timeout != 0)
        g_source_remove(tk->flash_timeout);

    if (tk->icon_pixbuf)
        g_object_unref(tk->icon_pixbuf);
    if (tk->icon_pixbuf_iconified)
        g_object_unref(tk->icon_pixbuf_iconified);

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

    taskbar_recompute_fold_by_count(tb);

    RET();
}

/******************************************************************************/

static gboolean task_update_thumbnail_preview_real(Task * tk)
{
    ENTER;
    tk->update_thumbnail_preview_idle = 0;

    if (tk->thumbnail_preview)
    {
        if (tk->thumbnail_preview)
            g_object_unref(G_OBJECT(tk->thumbnail_preview));
        tk->thumbnail_preview = NULL;
    }

    if (tk->thumbnail)
    {
        int preview_width = 150;
        int preview_height = 100;
        tk->thumbnail_preview = _gdk_pixbuf_scale_in_rect(tk->thumbnail, preview_width, preview_height, TRUE);
        if (tk->thumbnail_preview && tk->preview_image)
        {
            gtk_image_set_from_pixbuf(GTK_IMAGE(tk->preview_image), tk->thumbnail_preview);
        }
    }

    RET(FALSE);
}

static void task_update_thumbnail_preview(Task * tk)
{
    if (!tk->tb->thumbnails)
        return;

    if (tk->update_thumbnail_preview_idle == 0)
        tk->update_thumbnail_preview_idle = g_idle_add((GSourceFunc) task_update_thumbnail_preview_real, tk);
}

static gboolean task_update_composite_thumbnail_real(Task * tk)
{
    if (!tk->tb->thumbnails)
    {
        tk->update_composite_thumbnail_timeout = 0;
        return FALSE;
    }

    ENTER;

    if (tk->backing_pixmap != 0)
    {
        XFreePixmap(gdk_x11_get_default_xdisplay(), tk->backing_pixmap);
        tk->backing_pixmap = 0;
    }

    //Status status;

    gboolean skip = tk->iconified && tk->shaded;
    if (!skip)
    {
        XWindowAttributes window_attributes;
        window_attributes.map_state = IsUnmapped;
        /*status =*/ XGetWindowAttributes(gdk_x11_get_default_xdisplay(), tk->win, &window_attributes);
        if (window_attributes.map_state == IsUnmapped)
        {
            skip = TRUE;
        }
    }

    if (!skip)
    {
        Window w = tk->win;
        Window w1 = 0;

        Window root_return = 0;
        Window parent_return = 0;
        Window *children_return = NULL;
        unsigned int nchildren_return;

        /*status =*/ XQueryTree(gdk_x11_get_default_xdisplay(), w, &root_return, &parent_return, &children_return, &nchildren_return);
        if (children_return)
            XFree(children_return);

        //g_print("0x%x => 0x%x, (root 0x%x)\n", w, parent_return, root_return);

        if (parent_return != root_return)
            w1 = parent_return;

        if (w1)
           w = w1;

        if (w)
        {
            XWindowAttributes window_attributes;
            window_attributes.map_state = IsUnmapped;
            /*status =*/ XGetWindowAttributes(gdk_x11_get_default_xdisplay(), w, &window_attributes);
            if (window_attributes.map_state != IsUnmapped)
            {
                tk->backing_pixmap = XCompositeNameWindowPixmap(gdk_x11_get_default_xdisplay(), w);
            }
        }

    }
    //g_print("> %d %d\n", (int)tk->win, (int)tk->backing_pixmap);

    if (!skip && tk->backing_pixmap != 0)
    {
        GdkPixbuf * pixbuf = _gdk_pixbuf_get_from_pixmap(tk->backing_pixmap, -1, -1);
        if (pixbuf)
        {
            if (tk->thumbnail)
                g_object_unref(G_OBJECT(tk->thumbnail));
            if (tk->thumbnail_icon)
                g_object_unref(G_OBJECT(tk->thumbnail_icon));
            if (tk->thumbnail_preview)
                g_object_unref(G_OBJECT(tk->thumbnail_preview));

            tk->thumbnail = pixbuf;
            tk->thumbnail_icon = NULL;
            tk->thumbnail_preview = NULL;

            if (tk->tb->use_thumbnails_as_icons)
                task_defer_update_icon(tk, TRUE);
            if (tk->tb->thumbnails_preview)
                task_update_thumbnail_preview(tk);

            tk->require_update_composite_thumbnail = FALSE;

            //g_print("New thumb for [%s]\n", tk->name);
        }
    }

    tk->update_composite_thumbnail_idle = 0;

    RET(FALSE);
}

static gboolean task_update_composite_thumbnail_timeout(Task * tk)
{
    ENTER;

    if (!tk->tb->thumbnails || !tk->require_update_composite_thumbnail)
    {
        tk->update_composite_thumbnail_timeout = 0;
        RET(FALSE);
    }

    tk->update_composite_thumbnail_repeat_count++;
    if (tk->update_composite_thumbnail_repeat_count > 5)
    {
        tk->update_composite_thumbnail_repeat_count = 0;
        tk->require_update_composite_thumbnail = FALSE;
    }


    if (tk->update_composite_thumbnail_idle == 0)
        tk->update_composite_thumbnail_idle = g_idle_add((GSourceFunc) task_update_composite_thumbnail_real, tk);

    RET(TRUE);
}

static void task_update_composite_thumbnail(Task * tk)
{
    if (!tk->tb->thumbnails)
        return;

    tk->require_update_composite_thumbnail = TRUE;

    if (tk->update_composite_thumbnail_timeout == 0)
        tk->update_composite_thumbnail_timeout = g_timeout_add(1000 + rand() % 1000,
            (GSourceFunc) task_update_composite_thumbnail_timeout, tk);
}

static gboolean task_update_bgcolor_idle(Task * tk)
{
    TaskbarPlugin * tb = tk->tb;

    GdkColor * c1 = NULL;
    GdkColor * c2 = NULL;

    if (tk->icon_for_bgcolor && tb->colorize_buttons)
    {
        _gdk_pixbuf_get_color_sample(tk->icon_for_bgcolor, &tk->bgcolor1, &tk->bgcolor2);

        if (!tb->color_map)
            tb->color_map = panel_get_color_map(plugin_panel(tb->plug));

        if (tk->bgcolor1.pixel)
        {
            gdk_colormap_free_colors(tb->color_map, &tk->bgcolor1, 1);
            tk->bgcolor1.pixel = 0;
        }
        if (tk->bgcolor2.pixel)
        {
            gdk_colormap_free_colors(tb->color_map, &tk->bgcolor2, 1);
            tk->bgcolor2.pixel = 0;
        }

        gdk_colormap_alloc_color(tb->color_map, &tk->bgcolor1, FALSE, TRUE);
        gdk_colormap_alloc_color(tb->color_map, &tk->bgcolor2, FALSE, TRUE);

        if (tk->bgcolor1.pixel && tk->bgcolor2.pixel)
        {
            c1 = &tk->bgcolor1;
            c2 = &tk->bgcolor2;
        }

    }

    gtk_widget_modify_bg(GTK_WIDGET(tk->button), GTK_STATE_NORMAL, c1);
    gtk_widget_modify_bg(GTK_WIDGET(tk->button), GTK_STATE_ACTIVE, c1);
    gtk_widget_modify_bg(GTK_WIDGET(tk->button), GTK_STATE_PRELIGHT, c2);

    if (tk->icon_for_bgcolor)
    {
        g_object_unref(G_OBJECT(tk->icon_for_bgcolor));
        tk->icon_for_bgcolor = NULL;
    }

    tk->update_bgcolor_cb = 0;

    return FALSE;
}

static GdkPixbuf * get_window_icon(Task * tk, int icon_size, Atom source)
{
    TaskbarPlugin * tb = tk->tb;

    GdkPixbuf * pixbuf = NULL;

    /* Try to get an icon from the window manager at first */
    pixbuf = get_wm_icon(tk->win, icon_size, icon_size, source, &tk->image_source);

    if (!pixbuf && tk->wm_class)
    {
        /* try to guess an icon from window class name */
        gchar* classname = g_utf8_strdown(tk->wm_class, -1);
        pixbuf = lxpanel_load_icon(classname,
                                   icon_size, icon_size, FALSE);
        g_free(classname);
    }

    if (!pixbuf)
    {
         /* custom fallback icon */
         if (!tb->custom_fallback_pixbuf && !strempty(tb->custom_fallback_icon))
         {
             tb->custom_fallback_pixbuf = lxpanel_load_icon(tb->custom_fallback_icon, tb->icon_size, tb->icon_size, TRUE);
         }
         if (tb->custom_fallback_pixbuf)
         {
            pixbuf = tb->custom_fallback_pixbuf;
            g_object_ref(pixbuf);
         }
    }

    if (!pixbuf)
    {
         /* default fallback icon */
         if (!tb->fallback_pixbuf)
             tb->fallback_pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) icon_xpm);
         if (tb->fallback_pixbuf)
         {
            pixbuf = tb->fallback_pixbuf;
            g_object_ref(pixbuf);
         }
    }


    if (tk->icon_for_bgcolor)
        g_object_unref(G_OBJECT(tk->icon_for_bgcolor));
    g_object_ref(pixbuf);
    tk->icon_for_bgcolor = pixbuf;

    if (!tk->update_bgcolor_cb)
        tk->update_bgcolor_cb = g_idle_add((GSourceFunc) task_update_bgcolor_idle, tk);

    return pixbuf;
}


/* Update the icon of a task. */
static void task_create_icons(Task * tk, Atom source, int icon_size)
{
    TaskbarPlugin * tb = tk->tb;

    if (tk->icon_pixbuf)
        return;

    ENTER;

    GdkPixbuf * pixbuf = NULL;

    if (tb->thumbnails && tb->use_thumbnails_as_icons)
    {
        if (!tk->thumbnail_icon && tk->thumbnail)
            tk->thumbnail_icon =  _gdk_pixbuf_scale_in_rect(tk->thumbnail, icon_size, icon_size, TRUE);

        if (tk->thumbnail_icon)
        {
            pixbuf = tk->thumbnail_icon;
            g_object_ref(pixbuf);

            int s =
                (icon_size < 30) ? 0 : icon_size / 3;
            if (s)
            {
                GdkPixbuf * p1 = get_window_icon(tk, s, source);
                GdkPixbuf * p2 = _composite_thumb_icon(pixbuf, p1, icon_size, s);
                g_object_unref(p1);
                g_object_unref(pixbuf);
                pixbuf = p2;
            }
        }
    }

    if (!pixbuf)
    {
        pixbuf = get_window_icon(tk, icon_size, source);
        if (pixbuf)
        {
            GdkPixbuf * scaled_pixbuf =  _gdk_pixbuf_scale_in_rect(pixbuf, icon_size, icon_size, TRUE);
            g_object_unref(pixbuf);
            pixbuf = scaled_pixbuf;
        }
    }

    tk->icon_pixbuf = pixbuf;

    RET();

}

static void task_update_icon(Task * tk, Atom source, gboolean forse_icon_erase)
{
    forse_icon_erase |= tk->forse_icon_erase;

    tk->forse_icon_erase = FALSE;

    int icon_size = tk->tb->expected_icon_size > 0 ? tk->tb->expected_icon_size : tk->tb->icon_size;
    if (tk->allocated_icon_size > 0 && tk->allocated_icon_size < icon_size)
        icon_size = tk->allocated_icon_size;

    gboolean erase = (forse_icon_erase) || (icon_size != tk->icon_size);

    if (erase)
    {
        if (tk->icon_pixbuf)
        {
            g_object_unref(tk->icon_pixbuf);
            tk->icon_pixbuf = NULL;
        }
        if (tk->icon_pixbuf_iconified)
        {
            g_object_unref(tk->icon_pixbuf_iconified);
            tk->icon_pixbuf_iconified = NULL;
        }

        if (tk->thumbnail_icon)
        {
            g_object_unref(tk->thumbnail_icon);
            tk->thumbnail_icon = NULL;
        }
    }

    if (!tk->icon_pixbuf)
    {
        task_create_icons(tk, source, icon_size);
        tk->icon_size = icon_size;
    }

    if (tk->icon_pixbuf)
    {
        GdkPixbuf * pixbuf = tk->icon_pixbuf;
        if (tk->tb->dimm_iconified && tk->iconified)
        {
            if (!tk->icon_pixbuf_iconified)
            {
                tk->icon_pixbuf_iconified = gdk_pixbuf_add_alpha(tk->icon_pixbuf, FALSE, 0, 0, 0);;
                _wnck_dimm_icon(tk->icon_pixbuf_iconified);
            }
            pixbuf = tk->icon_pixbuf_iconified;
        }
        gtk_image_set_from_pixbuf(GTK_IMAGE(tk->image), pixbuf);
    }

}

static gboolean task_update_icon_cb(Task * tk)
{
    tk->update_icon_idle_cb = 0;
    task_update_icon(tk, None, TRUE);
    return FALSE;
}

static void task_defer_update_icon(Task * tk, gboolean forse_icon_erase)
{
    tk->forse_icon_erase |= forse_icon_erase;
    if (tk->update_icon_idle_cb == 0)
        tk->update_icon_idle_cb = g_idle_add((GSourceFunc) task_update_icon_cb, tk);
}


/* Timer expiration for urgency notification.  Also used to draw the button in setting and clearing urgency. */
static gboolean flash_window_timeout(Task * tk)
{
    /* Set state on the button and redraw. */
    if ( !task_button_is_really_flat(tk) )
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
    TaskClass * tc = tk->task_class;
    if (task_is_folded(tk))
        recompute_group_visibility_for_class(tb, tc);
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
    TaskClass * tc = tk->task_class;
    if (task_is_folded(tk))
        recompute_group_visibility_for_class(tb, tc);
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

/* group menu */

/* Remove the grouped-task popup menu from the screen. */
static void taskbar_group_menu_destroy(TaskbarPlugin * tb)
{
    ENTER;

    if (tb->hide_popup_delay_timer != 0)
    {
	g_source_remove(tb->hide_popup_delay_timer);
	tb->hide_popup_delay_timer = 0;
    }

    tb->group_menu_opened_as_popup = FALSE;

    if (tb->group_menu != NULL)
    {
        gtk_widget_destroy(tb->group_menu);
        tb->group_menu = NULL;
    }

    RET();
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
        } else if (tk_cursor->focused) {
            name = g_strdup_printf("* %s *", name);
            mi = gtk_image_menu_item_new_with_label(name);
            g_free(name);
        } else {
            mi = gtk_image_menu_item_new_with_label(name);
        }

        GtkWidget * im = gtk_image_new_from_pixbuf(gtk_image_get_pixbuf(GTK_IMAGE(tk_cursor->image)));
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), im);

        g_signal_connect(mi, "button_press_event", G_CALLBACK(taskbar_popup_activate_event), (gpointer) tk_cursor);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    }
}


/* Handler for "leave" event from group menu. */
static gboolean group_menu_motion(GtkWidget * widget, GdkEvent  *event, TaskbarPlugin * tb)
{
    taskbar_check_hide_popup(tb);
    return FALSE;
}

static void group_menu_size_allocate(GtkWidget * w, GtkAllocation * alloc, TaskbarPlugin * tb)
{
    tb->group_menu_alloc = *alloc;
}

static void task_show_window_list(Task * tk, GdkEventButton * event, gboolean similar, gboolean menu_opened_as_popup)
{
    ENTER;

    TaskbarPlugin * tb = tk->tb;
    TaskClass * tc = tk->task_class;

    GtkWidget * menu = gtk_menu_new();
    Task * tk_cursor;

    if (similar && task_is_folded(tk))
    {
        if (tc)
        {
            for (tk_cursor = tc->task_class_head; tk_cursor != NULL; tk_cursor = tk_cursor->task_class_flink)
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
            if (!similar || tk_cursor->task_class == tc)
                task_show_window_list_helper(tk_cursor, menu, tb);
        }
    }

    if (menu_opened_as_popup)
    {
        g_signal_connect_after(G_OBJECT (menu), "motion-notify-event", G_CALLBACK(group_menu_motion), (gpointer) tb);
        g_signal_connect(G_OBJECT (menu), "size-allocate", G_CALLBACK(group_menu_size_allocate), (gpointer) tb);
        tb->group_menu_opened_as_popup = TRUE;
    }

    /* Destroy already opened menu, if any. */
    taskbar_group_menu_destroy(tb);
    gtk_menu_popdown(GTK_MENU(tb->menu));

    /* Show the menu.  Set context so we can find the menu later to dismiss it.
     * Use a position-calculation callback to get the menu nicely positioned with respect to the button. */
    gtk_widget_show_all(menu);
    tb->group_menu = menu;

    guint event_button = event ? event->button : 0;
    guint32 event_time = event ? event->time: gtk_get_current_event_time();

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
        (GtkMenuPositionFunc) taskbar_popup_set_position, (gpointer) tk, event_button, event_time);

    RET();
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
    XIconifyWindow(gdk_x11_get_default_xdisplay(), tk->win, DefaultScreen(gdk_x11_get_default_xdisplay()));
}

static void task_raiseiconify(Task * tk, GdkEventButton * event)
{
    /*
     * If the task is iconified, raise it.
     * If the task is not iconified and has focus, iconify it.
     * If the task is not iconified and does not have focus, raise it. */
    if (tk->iconified)
        task_raise_window(tk, event ? event->time : gtk_get_current_event_time());
    else if ((tk->focused) || (tk == tk->tb->focused_previous))
        task_iconify(tk);
    else
        task_raise_window(tk, event ? event->time : gtk_get_current_event_time());
}

static void task_maximize(Task* tk)
{
    GdkWindow * win = gdk_x11_window_foreign_new_for_display(gdk_display_get_default(), tk->win);
    if (tk->maximized) {
        gdk_window_unmaximize(win);
    } else {
        gdk_window_maximize(win);
    }
    g_object_unref(G_OBJECT(win));
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
    set_decorations(tk->win, !tk->decorated);
    tk->decorated = !tk->decorated;
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
        event ? event->button : 0, event ? event->time : gtk_get_current_event_time());
}


static void task_activate_neighbour(Task * tk, GdkEventButton * event, gboolean next, gboolean in_group)
{
    gboolean before = TRUE;
    Task * candidate_before = NULL;
    Task * candidate_after = NULL;

    Task * tk_cursor;

    if (!tk)
        return;

    if (in_group && tk->task_class)
    {
        if (tk->tb->focused && tk->task_class == tk->tb->focused->task_class)
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
            && (!in_group || (tk->task_class && tk->task_class == tk_cursor->task_class));
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
    {
        task_raise_window(result, event ? event->time : gtk_get_current_event_time());
    }
    else if (in_group)
    {
        if (!tk->focused)
        {
            task_raise_window(tk, event ? event->time : gtk_get_current_event_time());
        }
    }
}

static void task_copy_title(Task * tk)
{
    GtkClipboard * clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, tk->name, strlen(tk->name));
    gtk_clipboard_store(clipboard);
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
        task_show_window_list(tk, event, FALSE, FALSE);
        break;
      case ACTION_SHOW_SIMILAR_WINDOW_LIST:
        task_show_window_list(tk, event, TRUE, FALSE);
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
      case ACTION_COPY_TITLE:
        task_copy_title(tk);
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
        tb->use_net_active = gdk_x11_screen_supports_net_wm_hint(gtk_widget_get_screen(plugin_widget(tb->plug)), net_active_atom);
        tb->net_active_checked = TRUE;
    }

    /* Raise the window.  We can use NET_ACTIVE_WINDOW if the window manager supports it.
     * Otherwise, do it the old way with XMapRaised and XSetInputFocus. */
    if (tk->tb->use_net_active)
        Xclimsg(tk->win, a_NET_ACTIVE_WINDOW, 2, time, 0, 0, 0);
    else
    {
        GdkWindow * gdkwindow = gdk_x11_window_lookup_for_display(gdk_display_get_default(), tk->win);
        if (gdkwindow != NULL)
            gdk_window_show(gdkwindow);
        else
            XMapRaised(gdk_x11_get_default_xdisplay(), tk->win);

	/* There is a race condition between the X server actually executing the XMapRaised and this code executing XSetInputFocus.
	 * If the window is not viewable, the XSetInputFocus will fail with BadMatch. */
	XWindowAttributes attr;
	XGetWindowAttributes(gdk_x11_get_default_xdisplay(), tk->win, &attr);
	if (attr.map_state == IsViewable)
            XSetInputFocus(gdk_x11_get_default_xdisplay(), tk->win, RevertToNone, time);
    }

    /* Change viewport if needed. */
    XWindowAttributes xwa;
    XGetWindowAttributes(gdk_x11_get_default_xdisplay(), tk->win, &xwa);
    Xclimsg(tk->win, a_NET_DESKTOP_VIEWPORT, xwa.x, xwa.y, 0, 0, 0);
}

/******************************************************************************/

/* preview panel */


static  gboolean preview_panel_configure_event (GtkWidget *widget, GdkEventConfigure *e, TaskbarPlugin * tb)
{
    tb->preview_panel_window_alloc.x = e->x;
    tb->preview_panel_window_alloc.y = e->y;
    tb->preview_panel_window_alloc.width = e->width;
    tb->preview_panel_window_alloc.height = e->height;

    //g_print("configure: %d, %d, %d, %d\n", e->x, e->y, e->width, e->height);

    return FALSE;
}

static void preview_panel_size_allocate(GtkWidget * w, GtkAllocation * alloc, TaskbarPlugin * tb)
{
    tb->preview_panel_window_alloc = *alloc;
}

static gboolean preview_panel_enter(GtkWidget * widget, GdkEvent * event, TaskbarPlugin * tb)
{
    taskbar_check_hide_popup(tb);
    return FALSE;
}

static gboolean preview_panel_leave(GtkWidget * widget, GdkEvent * event, TaskbarPlugin * tb)
{
    taskbar_check_hide_popup(tb);
    return FALSE;
}

static gboolean preview_panel_press_event(GtkWidget * widget, GdkEventButton * event, Task * tk)
{
    return taskbar_task_control_event(widget, event, tk, TRUE);
}

/*static gboolean preview_panel_release_event(GtkWidget * widget, GdkEventButton * event, Task * tk)
{
    return taskbar_task_control_event(widget, event, tk, TRUE);
}*/

static void preview_panel_calculate_speed(TaskbarPlugin * tb, int window_left, int window_right)
{
    gboolean h = (plugin_get_orientation(tb->plug) == ORIENT_HORIZ);

    int right_border = h ? gdk_screen_width() : gdk_screen_height();

    int border = 50;
    int left_border = 0;
    int x = tb->preview_panel_mouse_position;
    int speed = 0;

    if (x < left_border + border && window_left < left_border)
    {
        speed = MIN((left_border + border) - x, left_border - window_left);
        if (speed > border)
            speed = border;

        if (speed > 4)
            speed /= 4;
        else
            speed = 1;
    }
    else if (x > right_border - border && window_right > right_border)
    {
        speed = MAX((right_border - border) - x, right_border - window_right);
        if (speed < -border)
            speed = -border;

        if (speed < -4)
            speed /= 4;
        else
            speed = -1;
    }

    tb->preview_panel_speed = speed;

    //g_print("%d\n", speed);
}

static gboolean preview_panel_motion_timer(TaskbarPlugin * tb)
{
    if (tb->preview_panel_speed == 0 || !gtk_widget_get_visible(tb->preview_panel_window))
    {
        tb->preview_panel_motion_timer = 0;
        return FALSE;
    }

    gboolean h = (plugin_get_orientation(tb->plug) == ORIENT_HORIZ);

    gint x = 0;
    gint y = 0;
    gtk_window_get_position(GTK_WINDOW(tb->preview_panel_window), &x, &y);

    if (h)
        x += tb->preview_panel_speed;
    else
        y += tb->preview_panel_speed;

    gtk_window_move(GTK_WINDOW(tb->preview_panel_window), x, y);

    int window_left;
    int window_right;

    if (h)
    {
        window_left  = x;
        window_right = x + tb->preview_panel_window_alloc.width;
    }
    else
    {
        window_left  = y;
        window_right = y + tb->preview_panel_window_alloc.height;
    }

    preview_panel_calculate_speed(tb, window_left, window_right);

    return TRUE;
}

static gboolean preview_panel_motion_event(GtkWidget * widget, GdkEventMotion * event, TaskbarPlugin * tb)
{
    gboolean h = (plugin_get_orientation(tb->plug) == ORIENT_HORIZ);

    int window_left;
    int window_right;

    if (h)
    {
        window_left  = tb->preview_panel_window_alloc.x;
        window_right = tb->preview_panel_window_alloc.x + tb->preview_panel_window_alloc.width;
        tb->preview_panel_mouse_position = event->x_root;
    }
    else
    {
        window_left  = tb->preview_panel_window_alloc.y;
        window_right = tb->preview_panel_window_alloc.y + tb->preview_panel_window_alloc.height;
        tb->preview_panel_mouse_position = event->y_root;
    }

    preview_panel_calculate_speed(tb, window_left, window_right);

    if (tb->preview_panel_speed && tb->preview_panel_motion_timer == 0)
    {
        tb->preview_panel_motion_timer = g_timeout_add(10, (GSourceFunc) preview_panel_motion_timer, tb);
    }

    return FALSE;
}

static void taskbar_build_preview_panel(TaskbarPlugin * tb)
{
    ENTER;

    GtkWidget * win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(win), 5);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
    gtk_window_stick(GTK_WINDOW(win));

    gtk_widget_set_can_focus(win, FALSE);

    g_signal_connect(G_OBJECT (win), "size-allocate", G_CALLBACK(preview_panel_size_allocate), (gpointer) tb);
    g_signal_connect(G_OBJECT (win), "configure-event",  G_CALLBACK(preview_panel_configure_event), (gpointer) tb);
    g_signal_connect_after(G_OBJECT (win), "enter-notify-event", G_CALLBACK(preview_panel_enter), (gpointer) tb);
    g_signal_connect_after(G_OBJECT (win), "leave-notify-event", G_CALLBACK(preview_panel_leave), (gpointer) tb);
    g_signal_connect_after(G_OBJECT (win), "motion-notify-event", G_CALLBACK(preview_panel_motion_event), (gpointer) tb);

    gtk_widget_realize(win);

//    wm_noinput(GDK_WINDOW_XWINDOW(win->window));
    gdk_window_set_accept_focus(gtk_widget_get_window(win), FALSE);

/*
    Atom state[3];
    state[0] = a_NET_WM_STATE_SKIP_PAGER;
    state[1] = a_NET_WM_STATE_SKIP_TASKBAR;
    state[2] = a_NET_WM_STATE_STICKY;
    XChangeProperty(gdk_x11_get_default_xdisplay(), GDK_WINDOW_XWINDOW(win->window), a_NET_WM_STATE, XA_ATOM,
          32, PropModeReplace, (unsigned char *) state, 3);
*/

    tb->preview_panel_window = win;

    RET();
}

static void taskbar_hide_preview_panel(TaskbarPlugin * tb)
{
    if (tb->preview_panel_window)
    {
         gtk_widget_hide(tb->preview_panel_window);
    }
}

static void task_show_preview_panel(Task * tk)
{
    ENTER;

    TaskbarPlugin * tb = tk->tb;

    if (!tb->preview_panel_window)
    {
         taskbar_build_preview_panel(tb);
    }

    if (tb->preview_panel_box)
    {
        gtk_widget_destroy(tb->preview_panel_box);
        g_object_unref(G_OBJECT(tb->preview_panel_box));
        tb->preview_panel_box = NULL;
    }

    tb->preview_panel_box = gtk_event_box_new();
    g_object_ref(G_OBJECT(tb->preview_panel_box));
    //gtk_container_set_border_width(GTK_CONTAINER(tb->preview_panel_window), 5);
    gtk_container_add(GTK_CONTAINER(tb->preview_panel_window), tb->preview_panel_box);
/*
    if (tb->colorize_buttons)
    {
        if (tk->bgcolor1.pixel)
        {
            gtk_widget_modify_bg(tb->preview_panel_box, GTK_STATE_NORMAL, &tk->bgcolor1);
        }
    }
*/
    GtkWidget * box =
        (plugin_get_orientation(tb->plug) == ORIENT_HORIZ) ?
        gtk_hbox_new(TRUE, 5):
        gtk_vbox_new(TRUE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(box), 5);
    gtk_container_add(GTK_CONTAINER(tb->preview_panel_box), box);

    Task* tk_cursor = tk;
    if (tk->task_class)
        tk_cursor = tk->task_class->task_class_head;
    for (; tk_cursor; tk_cursor = tk_cursor->task_class_flink)
    {
        if (!task_is_visible_on_current_desktop(tk_cursor))
            continue;

        GtkWidget * button = gtk_toggle_button_new();
        gtk_container_set_border_width(GTK_CONTAINER(button), 0);

        GtkWidget * container = gtk_hbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(container), 0);

        if (tk->tb->tooltips)
            gtk_widget_set_tooltip_text(button, task_get_displayed_name(tk_cursor));

        GtkWidget * inner_container = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(inner_container), 0);
        gtk_box_pack_start(GTK_BOX(container), inner_container, TRUE, TRUE, 0);

        g_signal_connect(button, "button_press_event", G_CALLBACK(preview_panel_press_event), (gpointer) tk_cursor);
//        g_signal_connect(button, "button_release_event", G_CALLBACK(preview_panel_release_event), (gpointer) tk_cursor);

        if (tk_cursor->preview_image)
            g_object_unref(G_OBJECT(tk_cursor->preview_image));
        tk_cursor->preview_image = gtk_image_new_from_pixbuf(
            tk_cursor->thumbnail_preview ? tk_cursor->thumbnail_preview : tk_cursor->icon_pixbuf);
        g_object_ref(G_OBJECT(tk_cursor->preview_image));

        GtkWidget * image = tk_cursor->preview_image;

        GtkWidget * label = gtk_label_new(tk_cursor->name);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_size_request(label, 1, -1);
        gtk_box_pack_start(GTK_BOX(inner_container), label, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(inner_container), image, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(button), container);
        gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);

        if (tb->colorize_buttons)
        {
            if (tk_cursor->bgcolor1.pixel)
            {
                gtk_widget_modify_bg(button, GTK_STATE_NORMAL, &tk_cursor->bgcolor1);
                gtk_widget_modify_bg(button, GTK_STATE_ACTIVE, &tk_cursor->bgcolor1);
            }
            if (tk_cursor->bgcolor2.pixel)
            {
                gtk_widget_modify_bg(GTK_WIDGET(button), GTK_STATE_PRELIGHT, &tk_cursor->bgcolor2);
            }
        }

    }

    gtk_widget_show_all(tb->preview_panel_box);

    GtkRequisition requisition;
    gtk_widget_size_request(tb->preview_panel_window, &requisition);

    gint px, py;
    plugin_popup_set_position_helper2(tb->plug, tk->button, tb->preview_panel_window, &requisition, 5, 0.5, &px, &py);
    gtk_window_move(GTK_WINDOW(tb->preview_panel_window), px, py);

    gtk_widget_show_all(tb->preview_panel_window);

    gtk_window_move(GTK_WINDOW(tb->preview_panel_window), px, py);

    //g_print("%d, %d\n", px, py);

    RET();
}

/******************************************************************************/

/*

*** Handling of task mouse-hover popups ***

Opening:

button enter event -> start show_popup_delay_timer
button leave event -> stop  show_popup_delay_timer

show_popup_delay_timer event -> taskbar_show_popup_timeout() -> taskbar_show_popup()

taskbar_show_popup() calls task_show_preview_panel() or task_show_window_list() to do actual work.

Closing:

taskbar_check_hide_popup() is called every time we need to check whether mouse is within "leave popup open" area.
This are consists of popup window and task's button window.

mouse leaves the area -> start hide_popup_delay_timer
mouse is within the area -> stop hide_popup_delay_timer

hide_popup_delay_timer -> taskbar_hide_popup_timeout() -> taskbar_hide_popup()

taskbar_hide_popup() calls taskbar_group_menu_destroy or taskbar_hide_preview_panel to do actual work.

taskbar_hide_popup() is also called every time we need to force to close of the popup: from task_delete(),
on button press event and so on.

*/

static void taskbar_hide_popup_timeout(TaskbarPlugin * tb);

static void taskbar_check_hide_popup(TaskbarPlugin * tb)
{
    ENTER;

    gboolean from_group_menu = tb->group_menu_opened_as_popup;

    int x = 0, y = 0;
    gboolean out = FALSE;

    Task * tk = tb->popup_task;

    if (!tk)
    {
        taskbar_hide_popup(tb);
        RET();
    }

    gtk_widget_get_pointer(tk->button, &x, &y);
    out = x < 0 || y < 0 || x > tk->button_alloc.width || y > tk->button_alloc.height;

    if (out && from_group_menu && tb->group_menu)
    {
        gtk_widget_get_pointer(tb->group_menu, &x, &y);
        out = x < 0 || y < 0 || x > tb->group_menu_alloc.width || y > tb->group_menu_alloc.height;
    }

    if (out && !from_group_menu && tb->preview_panel_window)
    {
        gtk_widget_get_pointer(tb->preview_panel_window, &x, &y);
        out = x < 0 || y < 0 || x > tb->preview_panel_window_alloc.width || y > tb->preview_panel_window_alloc.height;
    }

    if (out)
    {
    	if (tb->hide_popup_delay_timer == 0)
	    tb->hide_popup_delay_timer =
		g_timeout_add(OPEN_GROUP_MENU_DELAY / 2, (GSourceFunc) taskbar_hide_popup_timeout, tb);
    }
    else
    {
	if (tb->hide_popup_delay_timer != 0)
	{
	    g_source_remove(tb->hide_popup_delay_timer);
	    tb->hide_popup_delay_timer = 0;
	}
    }


    RET();
}

static void taskbar_show_popup(Task * tk)
{
    TaskbarPlugin * tb = tk->tb;

    if (!tb->popup_task)
    {
        plugin_lock_visible(tb->plug);
    }

    tb->popup_task = tk;
    tk->show_popup_delay_timer = 0;
    if (tb->thumbnails_preview && tb->thumbnails)
        task_show_preview_panel(tk);
    else
        task_show_window_list(tk, NULL, TRUE, TRUE);
}

static void taskbar_hide_popup(TaskbarPlugin * tb)
{
    if (tb->hide_popup_delay_timer)
    {
        g_source_remove(tb->hide_popup_delay_timer);
        tb->hide_popup_delay_timer = 0;
    }

    if (tb->popup_task)
    {
         taskbar_hide_preview_panel(tb);
         taskbar_group_menu_destroy(tb);
         tb->popup_task = NULL;
         plugin_unlock_visible(tb->plug);
    }
}

static void taskbar_hide_popup_timeout(TaskbarPlugin * tb)
{
    tb->hide_popup_delay_timer = 0;
    taskbar_hide_popup(tb);
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

#if 0
static void debug_print_tasklist(Task * tk, char * text)
{
    Task* tk_cursor;
    TaskClass * tc = NULL;

    g_print("%s => ", text);

    gboolean space = FALSE;
    for (tk_cursor = tk->tb->task_list; tk_cursor != NULL; tk_cursor = tk_cursor->task_flink)
    {
        if (tc != tk_cursor->task_class)
        {
            if (tc)
                g_print(") ");
            if (tk_cursor->task_class)
                g_print("(");
            tc = tk_cursor->task_class;
            space = FALSE;
        }

        if (space)
            g_print(" ");

        if (tk_cursor != tk)
            g_print(" %2d ", tk_cursor->manual_order);
        else
            g_print("[%2d]", tk_cursor->manual_order);
        space = TRUE;
    }

    if (tc)
        g_print(")");

    g_print("\n");
}
#else
#define debug_print_tasklist(tk, text)
#endif

static gboolean taskbar_button_motion_notify_event(GtkWidget * widget, GdkEventMotion * event, Task * tk)
{
    if (tk->tb->button_pressed_task != tk)
        return FALSE;

    int old_manual_order = tk->manual_order;

    Task* tk_cursor;
    int manual_order = 0;
    gboolean before = TRUE;
    for (tk_cursor = tk->tb->task_list; tk_cursor != NULL; tk_cursor = tk_cursor->task_flink)
    {
        if (tk_cursor == tk)
            continue;

        if (!task_is_visible(tk_cursor))
        {
            tk_cursor->manual_order = manual_order++;
            continue;
        }

        int x = 0;
        gtk_widget_get_pointer(tk_cursor->button, &x, NULL);
        int mid = tk_cursor->button_alloc.width / 2;
        if (mid < 0)
            mid = 0;
        if (x >= mid)
        {
            tk_cursor->manual_order = manual_order++;
            continue;
        }

        if (before)
        {
            tk->manual_order = manual_order++;
            before = FALSE;
        }

        tk_cursor->manual_order = manual_order++;
    }

    if (before)
    {
        tk->manual_order = manual_order++;
        before = FALSE;
    }

    if (old_manual_order != tk->manual_order)
    {
        debug_print_tasklist(tk, "1");
        tk->tb->moving_task_now = TRUE;
        task_reorder(tk, FALSE);
        debug_print_tasklist(tk, "3");
        taskbar_redraw(tk->tb);
    }

    return TRUE;
}

/* Handler for "button-press-event" event from taskbar button,
 * or "activate" event from grouped-task popup menu item. */
static gboolean taskbar_task_control_event(GtkWidget * widget, GdkEventButton * event, Task * tk, gboolean popup_menu)
{
    TaskbarPlugin * tb = tk->tb;

    /* Close button pressed? {{ */

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
    if (event_in_close_button)
        return TRUE;

    /* }} */


    /* Get action for event. */

    int action = ACTION_NONE;
    switch (event->button) {
        case 1: action = (event->state & GDK_SHIFT_MASK) ? tb->shift_button1_action : tb->button1_action; break;
        case 2: action = (event->state & GDK_SHIFT_MASK) ? tb->shift_button2_action : tb->button2_action; break;
        case 3: action = (event->state & GDK_SHIFT_MASK) ? tb->shift_button3_action : tb->button3_action; break;
    }

    /* Action shows menu? */

    gboolean action_opens_menu = FALSE;
    switch (action) {
        case ACTION_MENU:
        case ACTION_SHOW_WINDOW_LIST:
        case ACTION_SHOW_SIMILAR_WINDOW_LIST:
            action_opens_menu = TRUE;
    }

    /* Event really triggers action? */

    gboolean click = FALSE;

    if (popup_menu) {
        /* Event from popup => trigger action. */
        click = TRUE;
        tb->button_pressed_task = NULL;
        tb->moving_task_now = FALSE;
    } else if ( (action_opens_menu && tb->menu_actions_click_press) || (!action_opens_menu && tb->other_actions_click_press) ) {
        /* Action triggered with button press */
        if (event->type == GDK_BUTTON_PRESS)
            click = TRUE;
    } else if (event->type == GDK_BUTTON_PRESS) {
        /* Action triggered with button click => remember that we receive button press */
        tb->button_pressed_task = tk;
        tb->moving_task_now = FALSE;
    } else if (event->type == GDK_BUTTON_RELEASE) {
        /* Action triggered with button click => trigger it */
        click = tk->entered_state && tb->button_pressed_task == tk && !tb->moving_task_now;
        tb->button_pressed_task = NULL;
        tb->moving_task_now = FALSE;
    }

    task_button_redraw_button_state(tk, tb);

    if (!click)
        return TRUE;


    /* Do real work. */

    if (task_is_folded(tk) && (!popup_menu))
    {
        /* If this is a grouped-task representative, meaning that there is a class with at least two windows,
         * bring up a popup menu listing all the class members. */
        task_show_window_list(tk, event, TRUE, FALSE);
    }
    else
    {
        /* Not a grouped-task representative, or entered from the grouped-task popup menu. */

        Task * visible_task = (
            (tb->single_window) ? tb->focused :
            (!task_is_folded(tk)) ? tk :
            (tk->task_class) ? tk->task_class->visible_task :
            tk);
        taskbar_group_menu_destroy(tb);

        if (popup_menu && (action == ACTION_SHOW_SIMILAR_WINDOW_LIST || action == ACTION_SHOW_WINDOW_LIST))
            action = ACTION_RAISEICONIFY;
        task_action(tk, action, event, visible_task, popup_menu);
    }

    /* As a matter of policy, avoid showing selected or prelight states on flat buttons. */
    if (task_button_is_really_flat(tk))
        gtk_widget_set_state(widget, GTK_STATE_NORMAL);
    return TRUE;
}

/* Handler for "button-press-event" event from taskbar button. */
static gboolean taskbar_button_press_event(GtkWidget * widget, GdkEventButton * event, Task * tk)
{
    taskbar_hide_popup(tk->tb);

    if (event->state & GDK_CONTROL_MASK && event->button == 3) {
        Plugin* p = tk->tb->plug;
        plugin_show_menu( p, event );
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

/* Handler for group menu timeout. */
static gboolean taskbar_show_popup_timeout(Task * tk)
{
    taskbar_show_popup(tk);
    return FALSE;
}

/* Handler for "enter" event from taskbar button.  This indicates that the cursor position has entered the button. */
static void taskbar_button_enter(GtkWidget * widget, Task * tk)
{
    TaskbarPlugin * tb = tk->tb;

    tk->entered_state = TRUE;
    if (task_button_is_really_flat(tk))
        gtk_widget_set_state(widget, GTK_STATE_NORMAL);
    task_draw_label(tk);

    gboolean popup = (tb->thumbnails_preview && tb->thumbnails);
    popup |= (tb->open_group_menu_on_mouse_over && task_class_is_folded(tb, tk->task_class));
    if (popup)
    {
	if (tk->show_popup_delay_timer == 0)
	    tk->show_popup_delay_timer =
		g_timeout_add(OPEN_GROUP_MENU_DELAY, (GSourceFunc) taskbar_show_popup_timeout, tk);
    }
}

/* Handler for "leave" event from taskbar button.  This indicates that the cursor position has left the button. */
static void taskbar_button_leave(GtkWidget * widget, Task * tk)
{
    TaskbarPlugin * tb = tk->tb;

    if (tb->popup_task)
    {
        taskbar_check_hide_popup(tb);
    }

    if (tk->show_popup_delay_timer != 0)
    {
        g_source_remove(tk->show_popup_delay_timer);
        tk->show_popup_delay_timer = 0;
    }

    tk->entered_state = FALSE;
    task_draw_label(tk);
    task_button_redraw_button_state(tk, tb);
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

    if (gtk_widget_get_realized(btn))
    {
        /* Get the coordinates of the button. */
        int x, y;
        gdk_window_get_origin(GTK_BUTTON(btn)->event_window, &x, &y);

        /* Send a NET_WM_ICON_GEOMETRY property change on the window. */
        if (tk->tb->use_x_net_wm_icon_geometry)
        {
            guint32 data[4];
            data[0] = x;
            data[1] = y;
            data[2] = alloc->width;
            data[3] = alloc->height;
            XChangeProperty(gdk_x11_get_default_xdisplay(), tk->win,
                gdk_x11_get_xatom_by_name("_NET_WM_ICON_GEOMETRY"),
                XA_CARDINAL, 32, PropModeReplace, (guchar *) &data, 4);
        }
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

    if (tk->allocated_icon_size != tk->icon_size)
        task_defer_update_icon(tk, FALSE);
}

/******************************************************************************/

/* Update style on the taskbar when created or after a configuration change. */
static void taskbar_update_style(TaskbarPlugin * tb)
{
    GtkOrientation bo = (plugin_get_orientation(tb->plug) == ORIENT_HORIZ) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    icon_grid_set_expand(tb->icon_grid, taskbar_task_button_is_expandable(tb));
    icon_grid_set_geometry(tb->icon_grid, bo,
        taskbar_get_task_button_max_width(tb), tb->icon_size + BUTTON_HEIGHT_EXTRA,
        tb->spacing, 0, panel_get_oriented_height_pixels(plugin_panel(tb->plug)));
}

/* Update style on a task button when created or after a configuration change. */
static void task_update_style(Task * tk, TaskbarPlugin * tb)
{
    gtk_widget_set_visible(tk->image, tb->show_icons);

    if (tb->show_titles) {
        if (!tk->label)
            task_build_gui_label(tb, tk);
        gtk_box_set_child_packing(GTK_BOX(tk->container), tk->image, FALSE, FALSE, 0, GTK_PACK_START);
        gtk_widget_show(tk->label);
    } else if (tk->label){
        gtk_box_set_child_packing(GTK_BOX(tk->container), tk->image, TRUE, TRUE, 0, GTK_PACK_START);
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
    if (!is_my_own_window(tk->win))
        XSelectInput(gdk_x11_get_default_xdisplay(), tk->win, PropertyChangeMask | StructureNotifyMask);

    /* Allocate a toggle button as the top level widget. */
    tk->button = gtk_toggle_button_new();
    gtk_container_set_border_width(GTK_CONTAINER(tk->button), 0);
    gtk_drag_dest_set(tk->button, 0, NULL, 0, 0);

    /* Connect signals to the button. */
    g_signal_connect(tk->button, "button_press_event", G_CALLBACK(taskbar_button_press_event), (gpointer) tk);
    g_signal_connect(tk->button, "button_release_event", G_CALLBACK(taskbar_button_release_event), (gpointer) tk);
    g_signal_connect(tk->button, "motion-notify-event", G_CALLBACK(taskbar_button_motion_notify_event), (gpointer) tk);
    g_signal_connect(G_OBJECT(tk->button), "drag-motion", G_CALLBACK(taskbar_button_drag_motion), (gpointer) tk);
    g_signal_connect(G_OBJECT(tk->button), "drag-leave", G_CALLBACK(taskbar_button_drag_leave), (gpointer) tk);
    g_signal_connect_after(G_OBJECT (tk->button), "enter", G_CALLBACK(taskbar_button_enter), (gpointer) tk);
    g_signal_connect_after(G_OBJECT (tk->button), "leave", G_CALLBACK(taskbar_button_leave), (gpointer) tk);
    g_signal_connect_after(G_OBJECT(tk->button), "scroll-event", G_CALLBACK(taskbar_button_scroll_event), (gpointer) tk);
    g_signal_connect(tk->button, "size-allocate", G_CALLBACK(taskbar_button_size_allocate), (gpointer) tk);

    gtk_widget_add_events(tk->button, GDK_POINTER_MOTION_MASK);

    /* Create a box to contain the application icon and window title. */
    tk->container = gtk_hbox_new(FALSE, 1);
    gtk_container_set_border_width(GTK_CONTAINER(tk->container), 0);

    /* Create an image to contain the application icon and add it to the box. */
    tk->image = gtk_image_new_from_pixbuf(NULL);
    gtk_misc_set_padding(GTK_MISC(tk->image), 0, 0);
    task_update_icon(tk, None, TRUE);
    gtk_widget_show(tk->image);
    gtk_box_pack_start(GTK_BOX(tk->container), tk->image, TRUE, TRUE, 0);

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
    gtk_widget_set_can_focus(tk->button, FALSE);
    gtk_widget_set_can_default(tk->button, FALSE);

    /* Update styles on the button. */
    task_update_style(tk, tb);

    /* Flash button for window with urgency hint. */
    if (tk->urgency)
        task_set_urgency(tk);
}

/******************************************************************************/

static void taskbar_update_separators(TaskbarPlugin * tb)
{
    if (!tb->use_group_separators)
        return;

    ENTER;

    Task * tk_cursor;
    Task * tk_prev = NULL;

    for (tk_cursor = tb->task_list; tk_cursor != NULL; tk_cursor = tk_cursor->task_flink)
    {
        if (!task_is_visible(tk_cursor))
            continue;

        if (tk_prev)
        {
            int separator = tk_prev->task_class != tk_cursor->task_class;
            if (separator != tk_prev->separator)
            {
                icon_grid_set_separator(tb->icon_grid, tk_prev->button, separator);
                tk_prev->separator = separator;
            }
        }
        tk_prev = tk_cursor;
    }

    RET();
}

/******************************************************************************/

/* Task reordering. */

static int task_compare(Task * tk1, Task * tk2)
{
    int result = 0;

    if (tk1->tb->rearrange)
    {
        if (tk1->tb->grouped_tasks)
        {
            int w1 = tk1->task_class ? tk1->task_class->manual_order : tk1->manual_order;
            int w2 = tk2->task_class ? tk2->task_class->manual_order : tk2->manual_order;
            result = w2 - w1;
        }
        if (result == 0)
            result = tk2->manual_order - tk1->manual_order;
        return result;
    }

    if (tk1->tb->grouped_tasks) switch (tk1->tb->_group_by)
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
        case GROUP_BY_NONE:
        {
            int w1 = tk1->task_class ? tk1->task_class->timestamp : INT_MAX;
            int w2 = tk2->task_class ? tk2->task_class->timestamp : INT_MAX;
            result = w2 - w1;
            break;
        }
    }

    int i;
    for (i = 0; i < 3; i++)
    {
        if (result != 0)
            break;

        switch (tk1->tb->sort_by[i])
        {
            case SORT_BY_TIMESTAMP:
            {
                result = tk2->timestamp - tk1->timestamp;
                break;
            }
            case SORT_BY_TITLE:
            {
                char * name1 = tk1->name ? tk1->name : "";
                char * name2 = tk2->name ? tk2->name : "";
                result = strcmp(name2, name1);
                break;
            }
            case SORT_BY_FOCUS:
            {
                result = tk2->focus_timestamp - tk1->focus_timestamp;
                break;
            }
            case SORT_BY_STATE:
            {
                int w1 = tk1->urgency * 2 + tk1->iconified;
                int w2 = tk2->urgency * 2 + tk2->iconified;
                result = w2 - w1;
                break;
            }
            case SORT_BY_WORKSPACE:
            {
                int w1 = (tk1->desktop == ALL_WORKSPACES) ? 0 : (tk1->desktop + 1);
                int w2 = (tk2->desktop == ALL_WORKSPACES) ? 0 : (tk2->desktop + 1);
                result = w2 - w1;
                break;
            }
        }
        if (tk1->tb->sort_reverse[i])
            result = -result;
    }

    return result;
}

/* FIXME: We MUST use double linked list instead of this crap. */
static Task * task_get_prev(Task * tk)
{
    TaskbarPlugin * tb = tk->tb;

    Task* tk_cursor = NULL;
    if (tb->task_list == tk)
        return NULL;

    for (tk_cursor = tb->task_list; tk_cursor != NULL; tk_cursor = tk_cursor->task_flink)
    {
        if (tk_cursor->task_flink == tk)
            return tk_cursor;
    }
    return NULL;
}

static void task_insert_after(Task * tk, Task * tk_prev_new)
{
    TaskbarPlugin * tb = tk->tb;

    Task* tk_prev_old = task_get_prev(tk);

    if (tk_prev_new) {
        if (tk_prev_old != tk_prev_new) {
            if (tk_prev_old)
                tk_prev_old->task_flink = tk->task_flink;
            else
                tb->task_list = tk->task_flink;
            tk->task_flink = tk_prev_new->task_flink;
            tk_prev_new->task_flink = tk;

            DBG("[0x%x] task \"%s\" (0x%x) moved after \"%s\" (0x%x)\n", tb, tk->name, tk, tk_prev_new->name, tk_prev_new);

            icon_grid_place_child_after(tb->icon_grid, tk->button, tk_prev_new->button);
        } else {
            DBG("[0x%x] task \"%s\" (0x%x) is in rigth place\n", tb, tk->name, tk);
        }
    } else {
        if (tk_prev_old)
            tk_prev_old->task_flink = tk->task_flink;
        else
            tb->task_list = tk->task_flink;

        tk->task_flink = tb->task_list;
        tb->task_list = tk;

        DBG("[0x%x] task \"%s\" (0x%x) moved to head\n", tb, tk->name, tk);

        icon_grid_place_child_after(tb->icon_grid, tk->button, NULL);
    }

}

static void task_reorder(Task * tk, gboolean and_others)
{
    Task* tk_cursor;
    TaskbarPlugin * tb = tk->tb;

    /*
      FIXME:
      There is nothing to do. We need to switch to Model-view-controller model
      and also implement a new widget container.
    */

    if (tb->rearrange && tb->grouped_tasks)
    {
        TaskClass * tc;
        for (tc = tb->task_class_list; tc != NULL; tc = tc->task_class_flink)
        {
            int class_manual_order = INT_MAX;
            if (tc == tk->task_class)
            {
                class_manual_order = tk->manual_order;
            }
            else
            {
                for (tk_cursor = tc->task_class_head; tk_cursor != NULL; tk_cursor = tk_cursor->task_class_flink)
                {
                    if (tk_cursor->manual_order < class_manual_order)
                        class_manual_order = tk_cursor->manual_order;
                }
            }
            tc->manual_order = class_manual_order;
        }
        and_others = TRUE;
    }

    debug_print_tasklist(tk, "2");


again: ;

    Task* tk_prev_new = NULL;
    for (tk_cursor = tb->task_list; tk_cursor != NULL; tk_cursor = tk_cursor->task_flink)
    {
        if (tk_cursor == tk)
            continue;
        DBG("[0x%x] [\"%s\" (0x%x), \"%s\" (0x%x)] => %d\n", tb, tk->name,tk, tk_cursor->name,tk_cursor, task_compare(tk, tk_cursor));

        if (task_compare(tk, tk_cursor) > 0)
            break;
        tk_prev_new = tk_cursor;
    }

    task_insert_after(tk, tk_prev_new);

    if (and_others)
    {
        tk = tk->tb->task_list;
        while (tk && tk->task_flink)
        {
             if (task_compare(tk, tk->task_flink) < 0)
             {
                 tk = tk->task_flink;
                 goto again;
             }
             tk = tk->task_flink;
        }
    }

    if (tb->rearrange)
    {
        int manual_order = 0;
        for (tk_cursor = tb->task_list; tk_cursor != NULL; tk_cursor = tk_cursor->task_flink)
        {
            tk_cursor->manual_order = manual_order++;
        }
    }

}

static void task_update_grouping(Task * tk, int group_by)
{
//    ENTER;
//    DBG("group_by = %d, tb->_group_by = %d\n", group_by, tk->tb->_group_by);
    if (tk->tb->_group_by == group_by || group_by < 0)
    {
        task_set_class(tk);
        task_reorder(tk, FALSE);
        taskbar_redraw(tk->tb);
    }
//    RET();
}

static void task_update_sorting(Task * tk, int sort_by)
{
//    ENTER;
    int i;
    for (i = 0; i < 3; i++)
    {
        if (tk->tb->sort_by[i] == sort_by || sort_by < 0)
        {
           task_reorder(tk, FALSE);
           taskbar_redraw(tk->tb);
           break;
        }
    }
//    RET();
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
                    tk->manual_order = INT_MAX / 4;
                    tk->click_on = NULL;
                    tk->present_in_client_list = TRUE;
                    tk->win = client_list[i];
                    tk->tb = tb;
                    tk->name_source = None;
                    tk->image_source = None;

                    tk->x_window_position = -1;

                    tk->iconified = (get_wm_state(tk->win) == IconicState);
                    tk->maximized = nws.maximized_vert || nws.maximized_horz;
                    tk->shaded    = nws.shaded;
                    tk->decorated = get_decorations(tk->win, &nws);

                    tk->desktop = get_net_wm_desktop(tk->win);
                    tk->override_class_name = (char*) -1;
                    if (tb->use_urgency_hint)
                        tk->urgency = task_has_urgency(tk);

                    task_update_wm_class(tk);
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
                    task_reorder(tk, FALSE);
                    task_update_composite_thumbnail(tk);
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
            else
                tk_pred->task_flink = tk_succ;
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
    if (f == panel_get_toplevel_xwindow(plugin_panel(tb->plug)))
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
        ntk->name_changed = FALSE;
        ntk->focus_timestamp = ++tb->task_timestamp;
        ntk->focused = TRUE;
        tb->focused = ntk;
        recompute_group_visibility_for_class(tb, ntk->task_class);
        task_button_redraw(ntk);
        task_update_sorting(ntk, SORT_BY_FOCUS);
        task_update_composite_thumbnail(ntk);
    }

    if (ctk != NULL)
    {
        task_button_redraw(ctk);
    }

    if (tb->_unfold_focused_group || tb->_show_single_group)
    {
        recompute_group_visibility_on_current_desktop(tb);
        taskbar_redraw(tb);
    }

    icon_grid_resume_updates(tb->icon_grid);

    taskbar_check_hide_popup(tb);
}

/* Set given desktop as current. */
static void taskbar_set_current_desktop(TaskbarPlugin * tb, int desktop)
{
    ENTER;

    icon_grid_defer_updates(tb->icon_grid);

    /* Store the local copy of current desktops.  Redisplay the taskbar. */
    tb->current_desktop = desktop;
    //g_print("[0x%x] taskbar_set_current_desktop %d\n", (int) tb, tb->current_desktop);
    recompute_group_visibility_on_current_desktop(tb);
    taskbar_redraw(tb);

    icon_grid_resume_updates(tb->icon_grid);

    RET();
}

/* Switch to deferred desktop and window. */
static gboolean taskbar_switch_desktop_and_window(TaskbarPlugin * tb)
{
    ENTER;

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

    RET(FALSE);
}

/* Handler for "current-desktop" event from root window listener. */
static void taskbar_net_current_desktop(GtkWidget * widget, TaskbarPlugin * tb)
{
    ENTER;

    int desktop = get_net_current_desktop();

    int desktop_switch_timeout = 350;

    /* If target desktop has visible tasks, use deferred switching to redice blinking. */
    if (desktop_switch_timeout > 0 && taskbar_has_visible_tasks_on_desktop(tb, desktop)) {
        tb->deferred_current_desktop = desktop;
        tb->deferred_desktop_switch_timer = g_timeout_add(desktop_switch_timeout, (GSourceFunc) taskbar_switch_desktop_and_window, (gpointer) tb);
    } else {
        taskbar_set_current_desktop(tb, desktop);
    }

    RET();
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
    ENTER;

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
            if (tb->deferred_desktop_switch_timer != 0) {
                g_source_remove(tb->deferred_desktop_switch_timer);
                tb->deferred_desktop_switch_timer = 0;
            }
            taskbar_switch_desktop_and_window(tb);
        }
    }

    RET();
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
        tb->desktop_names = NULL;

    /* Get the NET_DESKTOP_NAMES property. */
    tb->desktop_names = get_utf8_property_list(GDK_ROOT_WINDOW(), a_NET_DESKTOP_NAMES, &tb->number_of_desktop_names);
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
                    task_update_sorting(tk, SORT_BY_WORKSPACE);
                    taskbar_redraw(tb);
                }
                else if ((at == XA_WM_NAME) || (at == a_NET_WM_NAME) || (at == a_NET_WM_VISIBLE_NAME))
                {
                    /* Window changed name. */
                    task_set_names(tk, at);
                    if (tk->task_class != NULL)
                    {
                        /* A change to the window name may change the visible name of the class. */
                        recompute_group_visibility_for_class(tb, tk->task_class);
                        if (tk->task_class->visible_task != NULL)
                            task_draw_label(tk->task_class->visible_task);
                    }
                    task_update_sorting(tk, SORT_BY_TITLE);
                }
                else if (at == XA_WM_CLASS)
                {
                    /* Window changed class. */
                    task_update_wm_class(tk);
                    task_update_grouping(tk, GROUP_BY_CLASS);
                }
                else if (at == a_WM_STATE)
                {
                    /* Window changed state. */
                    gboolean iconified = get_wm_state(win) == IconicState;
                    if (tk->iconified != iconified)
                    {
                        tk->iconified = iconified;
                        /* Do not update task label, if we a waiting for deferred desktop switching. */
                        if (tb->deferred_current_desktop < 0)
                        {
                           tk->deferred_iconified_update = FALSE;
                           task_draw_label(tk);
                           task_update_icon(tk, None, FALSE);
                        }
                        else
                        {
                            if (tb->dimm_iconified)
                                tk->deferred_iconified_update = TRUE;
                        }
                        task_update_grouping(tk, GROUP_BY_STATE);
                        task_update_sorting(tk, SORT_BY_STATE);
                        task_update_composite_thumbnail(tk);
                    }
                }
                else if (at == XA_WM_HINTS)
                {
                    /* Window changed "window manager hints".
                     * Some windows set their WM_HINTS icon after mapping. */
                    //task_update_icon(tk, XA_WM_HINTS);
                    task_update_icon(tk, None, TRUE);

                    if (tb->use_urgency_hint)
                    {
                        tk->urgency = task_has_urgency(tk);
                        if (tk->urgency)
                            task_set_urgency(tk);
                        else
                            task_clear_urgency(tk);
                        task_update_grouping(tk, GROUP_BY_STATE);
                        task_update_sorting(tk, SORT_BY_STATE);
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
                        tk = NULL;
                    }
                    else
                    {
                        tk->maximized = nws.maximized_vert || nws.maximized_horz;
                        tk->shaded    = nws.shaded;
                        tk->decorated = get_decorations(tk->win, &nws);
                        task_update_composite_thumbnail(tk);
                    }
                }
                else if (at == a_MOTIF_WM_HINTS)
                {
                    tk->decorated = get_mvm_decorations(tk->win) != 0;
                }
                else if (at == a_NET_WM_ICON)
                {
                    /* Window changed EWMH icon. */
                    task_update_icon(tk, a_NET_WM_ICON, TRUE);
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
    XMapRaised(gdk_x11_get_default_xdisplay(), tb->menutask->win);
    taskbar_group_menu_destroy(tb);
}

static void menu_restore_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_maximize(tb->menutask);
    taskbar_group_menu_destroy(tb);
}

static void menu_maximize_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_maximize(tb->menutask);
    taskbar_group_menu_destroy(tb);
}

static void menu_iconify_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_iconify(tb->menutask);
    taskbar_group_menu_destroy(tb);
}

static void menu_roll_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_shade(tb->menutask);
    taskbar_group_menu_destroy(tb);
}

static void menu_undecorate_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_undecorate(tb->menutask);
    taskbar_group_menu_destroy(tb);
}

static void menu_move_to_workspace(GtkWidget * widget, TaskbarPlugin * tb)
{
    int num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "num"));
    set_net_wm_desktop(tb->menutask->win, num);
    taskbar_group_menu_destroy(tb);
}

static void menu_move_to_this_workspace(GtkWidget * widget, TaskbarPlugin * tb)
{
    set_net_wm_desktop(tb->menutask->win, tb->current_desktop);
    taskbar_group_menu_destroy(tb);
}

static void menu_ungroup_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_set_override_class(tb->menutask, NULL);
    taskbar_group_menu_destroy(tb);
}

static void menu_move_to_group(GtkWidget * widget, TaskbarPlugin * tb)
{
    TaskClass * tc = (TaskClass *)(g_object_get_data(G_OBJECT(widget), "class_name"));
    if (tc && tc->class_name)
    {
        char * name = g_strdup(tc->class_name);
        task_set_override_class(tb->menutask, name);
        g_free(name);
    }
    taskbar_group_menu_destroy(tb);
}

static void task_move_to_new_group_cb(char * value, gpointer p)
{
    Task * tk = (Task *) p;

    tk->new_group_dlg = NULL;

    if (value)
        task_set_override_class(tk, value),
        g_free(value);
}

static void menu_move_to_new_group(GtkWidget * widget, TaskbarPlugin * tb)
{
    Task * tk = tb->menutask;

    if (tk->new_group_dlg)
        gtk_widget_destroy(tk->new_group_dlg),
        tk->new_group_dlg = NULL;

    tk->new_group_dlg = create_entry_dialog(_("Move window to new group"), NULL, NULL, task_move_to_new_group_cb, tk);

    taskbar_group_menu_destroy(tb);
}

static void menu_unfold_group_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    TaskClass * tc = tb->menutask->task_class;
    if (tc)
    {
        tc->unfold = TRUE;
        tc->manual_unfold_state = TRUE;

        icon_grid_defer_updates(tb->icon_grid);
        recompute_group_visibility_on_current_desktop(tb);
        taskbar_redraw(tb);
        icon_grid_resume_updates(tb->icon_grid);
    }
    taskbar_group_menu_destroy(tb);
}

static void menu_fold_group_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    TaskClass * tc = tb->menutask->task_class;
    if (tc)
    {
        tc->unfold = FALSE;
        tc->manual_unfold_state = TRUE;

        icon_grid_defer_updates(tb->icon_grid);
        recompute_group_visibility_on_current_desktop(tb);
        taskbar_redraw(tb);
        icon_grid_resume_updates(tb->icon_grid);
    }
    taskbar_group_menu_destroy(tb);
}

static void menu_copy_title(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_copy_title(tb->menutask);
    taskbar_group_menu_destroy(tb);
}

static void menu_run_new(GtkWidget * widget, TaskbarPlugin * tb)
{
    gchar * p = tb->menutask->run_path;
    if (p && p != (gchar *)-1)
        lxpanel_launch_app(p, NULL, FALSE);
    taskbar_group_menu_destroy(tb);
}

static void menu_close_window(GtkWidget * widget, TaskbarPlugin * tb)
{
    task_close(tb->menutask);
    taskbar_group_menu_destroy(tb);
}

/******************************************************************************/

/* Context menu adjust functions. */

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
    for (tc = tk->tb->task_class_list; tc != NULL; tc = tc->task_class_flink)
    {
        if (tc->visible_count && tk->task_class != tc)
        {
            gchar * label = g_strdup((tc->class_name && strlen(tc->class_name) > 0) ? tc->class_name : _("(unnamed)"));
            GtkWidget * mi = gtk_menu_item_new_with_label(label);
            g_free(label);

            g_object_set_data(G_OBJECT(mi), "class_name", tc);
            g_signal_connect(mi, "activate", G_CALLBACK(menu_move_to_group), tk->tb);
            gtk_menu_shell_append(GTK_MENU_SHELL(move_to_group_menu), mi);
        }
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(move_to_group_menu), gtk_separator_menu_item_new());

    GtkWidget * mi = gtk_menu_item_new_with_label(_("New group"));
    g_signal_connect(mi, "activate", G_CALLBACK(menu_move_to_new_group), tk->tb);
    gtk_menu_shell_append(GTK_MENU_SHELL(move_to_group_menu), mi);

    gtk_widget_show_all(move_to_group_menu);

    adjust_separators(move_to_group_menu);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(tk->tb->move_to_group_menuitem), move_to_group_menu);
}

static void menu_set_sensitive(GtkWidget * widget, gboolean value)
{
    if (!widget)
        return;

    char * p = (char *) g_object_get_data(G_OBJECT(widget), "hide_inactive");
    gboolean hide_inactive = (p && strcmp(p, "true") == 0) ? TRUE : FALSE;
    if (hide_inactive)
    {
        gtk_widget_set_visible(widget, value);
    }
    else
    {
        gtk_widget_set_visible(widget, TRUE);
        gtk_widget_set_sensitive(widget, value);
    }
}

static void task_adjust_menu(Task * tk, gboolean from_popup_menu)
{
    TaskbarPlugin * tb = tk->tb;

    if (tb->workspace_submenu) {
        gtk_container_foreach(GTK_CONTAINER(tb->workspace_submenu), task_adjust_menu_workspace_callback, tk);
    }

    if (tb->move_to_this_workspace_menuitem) {
        menu_set_sensitive(GTK_WIDGET(tb->move_to_this_workspace_menuitem), tk->desktop != tb->current_desktop && tk->desktop >= 0);
    }

    gboolean manual_grouping = tb->manual_grouping && tb->grouped_tasks;

    if (manual_grouping)
    {
        task_adjust_menu_move_to_group(tk);
        if (tb->move_to_group_menuitem)
            gtk_widget_set_visible(GTK_WIDGET(tb->move_to_group_menuitem), TRUE);

        if (tb->ungroup_menuitem)
            menu_set_sensitive(GTK_WIDGET(tb->ungroup_menuitem),
                tk->task_class && tk->task_class->visible_count > 1);

        if (tb->unfold_group_menuitem)
            menu_set_sensitive(GTK_WIDGET(tb->unfold_group_menuitem),
                !tb->_show_single_group && tk->task_class && task_is_folded(tk));

        if (tb->fold_group_menuitem)
            menu_set_sensitive(GTK_WIDGET(tb->fold_group_menuitem),
                !tb->_show_single_group && tk->task_class && !task_is_folded(tk));
    }
    else
    {
        if (tb->move_to_group_menuitem)
            gtk_widget_set_visible(GTK_WIDGET(tb->move_to_group_menuitem), FALSE);
        if (tb->ungroup_menuitem)
            gtk_widget_set_visible(GTK_WIDGET(tb->ungroup_menuitem), FALSE);
        if (tb->unfold_group_menuitem)
            gtk_widget_set_visible(GTK_WIDGET(tb->unfold_group_menuitem), FALSE);
        if (tb->fold_group_menuitem)
            gtk_widget_set_visible(GTK_WIDGET(tb->fold_group_menuitem), FALSE);
    }


    if (tb->maximize_menuitem)
        menu_set_sensitive(GTK_WIDGET(tb->maximize_menuitem), !tk->maximized);
    if (tb->restore_menuitem)
        menu_set_sensitive(GTK_WIDGET(tb->restore_menuitem), tk->maximized);

    if (tb->iconify_menuitem)
        menu_set_sensitive(GTK_WIDGET(tb->iconify_menuitem), !tk->iconified);

    if (tb->undecorate_menuitem)
    {
        //gtk_widget_set_visible(GTK_WIDGET(tb->undecorate_menuitem), TRUE);
        menu_set_sensitive(GTK_WIDGET(tb->undecorate_menuitem), !tk->shaded || !tk->decorated);
    }

    if (tb->roll_menuitem)
    {
        //gtk_widget_set_visible(GTK_WIDGET(tb->roll_menuitem), TRUE);
        menu_set_sensitive(GTK_WIDGET(tb->roll_menuitem), tk->shaded || tk->decorated);
    }

    if (tb->run_new_menuitem)
    {
        if (tk->wm_class && tk->run_path == (gchar *)-1)
        {
            tk->run_path = g_find_program_in_path(tk->wm_class);
            if (!tk->run_path)
            {
                 gchar* classname = g_utf8_strdown(tk->wm_class, -1);
                 tk->run_path = g_find_program_in_path(classname);
                 g_free(classname);
            }
        }
#if GTK_CHECK_VERSION(2,16,0)
        if (tk->wm_class && tk->run_path && tk->run_path != (gchar *)-1)
        {
            gchar * name = g_strdup_printf(_("Run new %s"), tk->wm_class);
            gtk_menu_item_set_use_underline(GTK_MENU_ITEM(tb->run_new_menuitem), FALSE);
            gtk_menu_item_set_label(GTK_MENU_ITEM(tb->run_new_menuitem), name);
            g_free(name);
        }
#endif
        gtk_widget_set_visible(GTK_WIDGET(tb->run_new_menuitem), tk->run_path && tk->run_path != (gchar *)-1);
    }


    if (from_popup_menu) {
#if GTK_CHECK_VERSION(2,16,0)
        gtk_menu_item_set_use_underline(GTK_MENU_ITEM(tb->title_menuitem), FALSE);
        gtk_menu_item_set_label(GTK_MENU_ITEM(tb->title_menuitem), tk->name);
#else
        if (tb->title_menuitem) {
            gtk_widget_destroy(tb->title_menuitem);
            tb->title_menuitem = NULL;
        }
        tb->title_menuitem = gtk_menu_item_new_with_label(tk->name);
        gtk_menu_shell_prepend(GTK_MENU_SHELL(tb->menu), tb->title_menuitem);
#endif
        gtk_widget_set_sensitive(GTK_WIDGET(tb->title_menuitem), FALSE);
    }
    if (tb->title_menuitem)
        gtk_widget_set_visible(GTK_WIDGET(tb->title_menuitem), from_popup_menu);

    adjust_separators(tb->menu);
}

/******************************************************************************/

/* Functions to build task context menu. */

/* Helper function to create menu items for taskbar_make_menu() */
static GtkWidget * create_menu_item(TaskbarPlugin * tb, char * name, GCallback activate_cb, GtkWidget ** menuitem, gboolean hide_inactive)
{
    GtkWidget * mi = gtk_menu_item_new_with_mnemonic(name);
    gtk_menu_shell_append(GTK_MENU_SHELL(tb->menu), mi);
    if (activate_cb)
        g_signal_connect(G_OBJECT(mi), "activate", activate_cb, tb);
    if (hide_inactive)
    {
        g_object_set_data(G_OBJECT(mi), "hide_inactive", "true");
    }
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

    gchar * menu_description =
        "close2 raise @restore @maximize @iconify @roll @undecorate - "
        "@move_to_this_workspace move_to_workspace - "
        "@ungroup move_to_group - @unfold_group @fold_group - "
        "run_new - copy_title";

    tb->workspace_submenu = NULL;
    tb->move_to_this_workspace_menuitem = NULL;
    tb->restore_menuitem = NULL;
    tb->maximize_menuitem = NULL;
    tb->iconify_menuitem = NULL;
    tb->roll_menuitem = NULL;
    tb->undecorate_menuitem = NULL;
    tb->ungroup_menuitem = NULL;
    tb->move_to_group_menuitem = NULL;
    tb->unfold_group_menuitem = NULL;
    tb->fold_group_menuitem = NULL;
    tb->title_menuitem = NULL;
    tb->group_menu = NULL;

    /* Load menu configuration */

    if (!tb->menu_config)
    {
        tb->menu_config = read_list_from_config("plugins/taskbar/task-menu");
        if (!tb->menu_config || g_strv_length(tb->menu_config) == 0)
        {
            g_strfreev(tb->menu_config);
            tb->menu_config = g_strsplit_set(menu_description, " \t\n,", 0);
            if (!tb->menu_config)
                return;
        }
    }

    gchar ** elements = tb->menu_config;

    /* Create menu items. */

    int close2 = 0;

    int element_nr;
    for (element_nr = 0; elements[element_nr]; element_nr++)
    {
        gchar * element = elements[element_nr];

        gboolean hide_inactive = FALSE;
        if (element[0] != 0)
        {
            if (element[0] == '@')
            {
                element++;
                hide_inactive = TRUE;
            }
        }

        #define IF(v) if (strcmp(element, v) == 0)

        IF("") {
            /* nothing */
        } else IF("-") {
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        } else IF("raise") {
            create_menu_item(tb, _("_Raise"), (GCallback) menu_raise_window, NULL, hide_inactive);
        } else IF("restore") {
            create_menu_item(tb, _("R_estore"), (GCallback) menu_restore_window, &tb->restore_menuitem, hide_inactive);
        } else IF("maximize") {
            create_menu_item(tb, _("Ma_ximize"), (GCallback) menu_maximize_window, &tb->maximize_menuitem, hide_inactive);
        } else IF("iconify") {
            create_menu_item(tb, _("Ico_nify"), (GCallback) menu_iconify_window, &tb->iconify_menuitem, hide_inactive);
        } else IF("undecorate") {
            create_menu_item(tb, _("Un/_decorate"), (GCallback) menu_undecorate_window, &tb->undecorate_menuitem, hide_inactive);
        } else IF("roll") {
            create_menu_item(tb, _("_Roll up/down"), (GCallback) menu_roll_window, &tb->roll_menuitem, hide_inactive);
        } else IF("move_to_this_workspace") {
            if (tb->number_of_desktops > 1)
            {
                create_menu_item(tb, _("Move to this workspace"), (GCallback) menu_move_to_this_workspace, &tb->move_to_this_workspace_menuitem, hide_inactive);
            }
        } else IF("move_to_workspace") {
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
                mi = create_menu_item(tb, _("_Move to Workspace"), NULL, NULL, hide_inactive);
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), workspace_menu);

                tb->workspace_submenu = workspace_menu;
            }
        } else IF("ungroup") {
            create_menu_item(tb, _("_Ungroup"), (GCallback) menu_ungroup_window, &tb->ungroup_menuitem, hide_inactive);
        } else IF("move_to_group") {
            create_menu_item(tb, _("M_ove to Group"), NULL, &tb->move_to_group_menuitem, hide_inactive);
        } else IF("unfold_group") {
            create_menu_item(tb, _("Unfold _Group"), (GCallback) menu_unfold_group_window, &tb->unfold_group_menuitem, hide_inactive);
        } else IF("fold_group") {
            create_menu_item(tb, _("Fold _Group"), (GCallback) menu_fold_group_window, &tb->fold_group_menuitem, hide_inactive);
        } else IF("copy_title") {
            create_menu_item(tb, _("Cop_y title"), (GCallback) menu_copy_title, NULL, hide_inactive);
        } else IF("run_new") {
            create_menu_item(tb, _("_Run new"), (GCallback) menu_run_new, &tb->run_new_menuitem, hide_inactive);
        } else IF("close") {
            create_menu_item(tb, _("_Close Window"), (GCallback) menu_close_window, NULL, hide_inactive);
        } else IF("close2") {
            close2 = 1;
        } else {
            g_warning("Invalid window menu command: %s\n", element);
        }

        #undef IF

    }

    /* Add Close menu item.  By popular demand, we place this menu item closest to the cursor. */
    if (close2)
    {
        mi = gtk_menu_item_new_with_mnemonic (_("_Close Window"));
        if (panel_get_edge(plugin_panel(tb->plug)) != EDGE_BOTTOM)
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
    }

    /* Add window title menu item and separator. */

    mi = gtk_separator_menu_item_new();
    gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), mi);
#if GTK_CHECK_VERSION(2,16,0)
    mi = gtk_menu_item_new_with_mnemonic("");
    gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), mi);
    tb->title_menuitem = mi;
#endif
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
    TaskbarPlugin * tb = PRIV(p);

    /* Set up style for taskbar. */
    gtk_rc_parse_string(taskbar_rc);

    /* Allocate top level widget and set into Plugin widget pointer. */
    GtkWidget * pwid = gtk_event_box_new();
    plugin_set_widget(p, pwid);
    gtk_container_set_border_width(GTK_CONTAINER(pwid), 0);
    gtk_widget_set_has_window(pwid, FALSE);
    gtk_widget_set_name(pwid, "taskbar");

    /* Make container for task buttons as a child of top level widget. */
    GtkOrientation bo = (plugin_get_orientation(p) == ORIENT_HORIZ) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    tb->icon_grid = icon_grid_new(
         plugin_panel(p), plugin_widget(p), bo,
         tb->task_width_max, tb->icon_size, tb->spacing, 0, panel_get_oriented_height_pixels(plugin_panel(p)));
    icon_grid_set_expand(tb->icon_grid, taskbar_task_button_is_expandable(tb));
    icon_grid_use_separators(tb->icon_grid, tb->use_group_separators);
    icon_grid_set_separator_size(tb->icon_grid, tb->group_separator_size);
    taskbar_update_style(tb);

    /* Add GDK event filter. */
    gdk_window_add_filter(NULL, (GdkFilterFunc) taskbar_event_filter, tb);

    /* Connect signal to receive mouse events on the unused portion of the taskbar. */
    g_signal_connect(pwid, "button-press-event", G_CALLBACK(plugin_button_press_event), p);

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
    g_signal_connect(gtk_widget_get_screen(plugin_widget(p)), "window-manager-changed", G_CALLBACK(taskbar_window_manager_changed), tb);
}

static void taskbar_config_updated(TaskbarPlugin * tb)
{
    if (!tb->show_iconified)
        tb->show_mapped = TRUE;

    tb->open_group_menu_on_mouse_over = tb->mouse_over_action == MOUSE_OVER_ACTION_GROUP_MENU;
    tb->thumbnails_preview            = tb->mouse_over_action == MOUSE_OVER_ACTION_PREVIEW;

    int sort_settings_hash = 0;
    int i;
    for (i = 0; i < 3; i++)
    {
        int v = (tb->sort_by[i] + 1) * 2 + !!tb->sort_reverse[i];
        sort_settings_hash *= 20;
        sort_settings_hash += v;
    }

    sort_settings_hash += tb->rearrange ? 0 : 10000;

    if (tb->rearrange)
    {
        tb->manual_grouping = FALSE;
        tb->group_by = GROUP_BY_CLASS;
    }

    tb->grouped_tasks = tb->mode == MODE_GROUP;
    tb->single_window = tb->mode == MODE_SINGLE_WINDOW;

    int group_by = (tb->grouped_tasks) ? tb->group_by : GROUP_BY_NONE;

    tb->show_icons  = tb->show_icons_titles != SHOW_TITLES;
    tb->show_titles = tb->show_icons_titles != SHOW_ICONS;

    tb->rebuild_gui = tb->_mode != tb->mode;
    tb->rebuild_gui |= tb->show_all_desks_prev_value != tb->show_all_desks;
    tb->rebuild_gui |= tb->_group_fold_threshold != tb->group_fold_threshold;
    tb->rebuild_gui |= tb->_panel_fold_threshold != tb->panel_fold_threshold;
    tb->rebuild_gui |= tb->_group_by != group_by;
    tb->rebuild_gui |= tb->show_iconified_prev != tb->show_iconified;
    tb->rebuild_gui |= tb->show_mapped_prev != tb->show_mapped;
    tb->rebuild_gui |= tb->sort_settings_hash != sort_settings_hash;
    tb->rebuild_gui |= tb->colorize_buttons_prev != tb->colorize_buttons;

    if (tb->rebuild_gui) {
        tb->_mode = tb->mode;
        tb->show_all_desks_prev_value = tb->show_all_desks;
        tb->_group_fold_threshold = tb->group_fold_threshold;
        tb->_panel_fold_threshold = tb->panel_fold_threshold;
        tb->_group_by = group_by;
        tb->show_iconified_prev = tb->show_iconified;
        tb->show_mapped_prev = tb->show_mapped;
        tb->sort_settings_hash = sort_settings_hash;
        tb->colorize_buttons_prev = tb->colorize_buttons;
    }

    tb->_show_close_buttons = tb->show_close_buttons && !(tb->grouped_tasks && tb->_group_fold_threshold == 1);

    gboolean unfold_focused_group = tb->grouped_tasks && tb->unfold_focused_group;
    gboolean show_single_group = tb->grouped_tasks && tb->show_single_group;

    gboolean recompute_visibility = tb->_unfold_focused_group != unfold_focused_group;
    recompute_visibility |= tb->_show_single_group != show_single_group;
    recompute_visibility |= tb->use_group_separators_prev != tb->use_group_separators;

    tb->thumbnails = (tb->thumbnails_preview || tb->use_thumbnails_as_icons) && panel_is_composited(plugin_panel(tb->plug));

    if (tb->dimm_iconified_prev != tb->dimm_iconified)
    {
        tb->dimm_iconified_prev = tb->dimm_iconified;
        Task * tk;
        for (tk = tb->task_list; tk != NULL; tk = tk->task_flink)
        {
            tk->deferred_iconified_update = TRUE;
        }
        taskbar_redraw(tb);
    }

    if (tb->icon_grid)
    {
        icon_grid_use_separators(tb->icon_grid, tb->use_group_separators);
        icon_grid_set_separator_size(tb->icon_grid, tb->group_separator_size);
    }

    if (recompute_visibility)
    {
        tb->_unfold_focused_group = unfold_focused_group;
	tb->_show_single_group = show_single_group;
	tb->use_group_separators_prev = tb->use_group_separators;
        recompute_group_visibility_on_current_desktop(tb);
        taskbar_redraw(tb);
    }
}

/* Plugin constructor. */
static int taskbar_constructor(Plugin * p, char ** fp)
{
    atom_LXPANEL_TASKBAR_WINDOW_POSITION = XInternAtom( gdk_x11_get_default_xdisplay(), "_LXPANEL_TASKBAR_WINDOW_POSITION", False );

    /* Allocate plugin context and set into Plugin private data pointer. */
    TaskbarPlugin * tb = g_new0(TaskbarPlugin, 1);
    tb->plug = p;
    plugin_set_priv(p, tb);

    /* Initialize to defaults. */
    tb->icon_size         = plugin_get_icon_size(p);
    tb->tooltips          = TRUE;
    tb->show_icons_titles = SHOW_BOTH;
    tb->custom_fallback_icon = "xorg";
    tb->show_all_desks    = FALSE;
    tb->show_urgency_all_desks = TRUE;
    tb->show_mapped       = TRUE;
    tb->show_iconified    = TRUE;
    tb->task_width_max    = TASK_WIDTH_MAX;
    tb->spacing           = 1;
    tb->use_urgency_hint  = TRUE;
    tb->mode              = MODE_CLASSIC;
    tb->group_fold_threshold = 5;
    tb->panel_fold_threshold = 10;
    tb->group_by          = GROUP_BY_CLASS;
    tb->manual_grouping   = TRUE;
    tb->unfold_focused_group = FALSE;
    tb->show_single_group = FALSE;
    tb->show_close_buttons = FALSE;

    tb->highlight_modified_titles = FALSE;

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

    tb->menu_actions_click_press = TRIGGERED_BY_PRESS;
    tb->other_actions_click_press = TRIGGERED_BY_CLICK;

    tb->grouped_tasks     = FALSE;
    tb->single_window     = FALSE;
    tb->show_icons        = TRUE;
    tb->show_titles       = TRUE;
    tb->show_all_desks_prev_value = FALSE;
    tb->_show_close_buttons = FALSE;
    tb->extra_size        = 0;

    tb->colorize_buttons = FALSE;

    tb->workspace_submenu = NULL;
    tb->restore_menuitem  = NULL;
    tb->maximize_menuitem = NULL;
    tb->iconify_menuitem  = NULL;
    tb->roll_menuitem  = NULL;
    tb->undecorate_menuitem  = NULL;
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
                else if (g_ascii_strcasecmp(s.t[0], "FallbackIcon") == 0)
                    tb->custom_fallback_icon = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "ShowIconified") == 0)
                    tb->show_iconified = str2num(bool_pair, s.t[1], tb->show_iconified);
                else if (g_ascii_strcasecmp(s.t[0], "ShowMapped") == 0)
                    tb->show_mapped = str2num(bool_pair, s.t[1], tb->show_mapped);
                else if (g_ascii_strcasecmp(s.t[0], "ShowAllDesks") == 0)
                    tb->show_all_desks = str2num(bool_pair, s.t[1], tb->show_all_desks);
                else if (g_ascii_strcasecmp(s.t[0], "ShowUrgencyAllDesks") == 0)
                    tb->show_urgency_all_desks = str2num(bool_pair, s.t[1], tb->show_urgency_all_desks);
                else if (g_ascii_strcasecmp(s.t[0], "MaxTaskWidth") == 0)
                    tb->task_width_max = atoi(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "spacing") == 0)
                    tb->spacing = atoi(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "UseMouseWheel") == 0)              /* For backward compatibility */
                    ;
                else if (g_ascii_strcasecmp(s.t[0], "UseUrgencyHint") == 0)
                    tb->use_urgency_hint = str2num(bool_pair, s.t[1], tb->use_urgency_hint);
                else if (g_ascii_strcasecmp(s.t[0], "DimmIconified") == 0)
                    tb->dimm_iconified = str2num(bool_pair, s.t[1], tb->dimm_iconified);
                else if (g_ascii_strcasecmp(s.t[0], "ColorizeButtons") == 0)
                    tb->colorize_buttons = str2num(bool_pair, s.t[1], tb->colorize_buttons);
                else if (g_ascii_strcasecmp(s.t[0], "IconThumbnails") == 0)
                    tb->use_thumbnails_as_icons = str2num(bool_pair, s.t[1], tb->use_thumbnails_as_icons);
                else if (g_ascii_strcasecmp(s.t[0], "FlatButton") == 0)
				{
                    tb->flat_inactive_buttons = str2num(bool_pair, s.t[1], tb->flat_inactive_buttons);
					tb->flat_active_button    = str2num(bool_pair, s.t[1], tb->flat_active_button);
				}
                else if (g_ascii_strcasecmp(s.t[0], "FlatInactiveButtons") == 0)
                    tb->flat_inactive_buttons = str2num(bool_pair, s.t[1], tb->flat_inactive_buttons);
                else if (g_ascii_strcasecmp(s.t[0], "FlatActiveButton") == 0)
                    tb->flat_active_button = str2num(bool_pair, s.t[1], tb->flat_active_button);
                else if (g_ascii_strcasecmp(s.t[0], "BoldFontOnMouseOver") == 0)
                    tb->bold_font_on_mouse_over = str2num(bool_pair, s.t[1], tb->bold_font_on_mouse_over);
                else if (g_ascii_strcasecmp(s.t[0], "GroupedTasks") == 0)		/* For backward compatibility */
                    ;
                else if (g_ascii_strcasecmp(s.t[0], "Mode") == 0)
                    tb->mode = str2num(mode_pair, s.t[1], tb->mode);
                else if (g_ascii_strcasecmp(s.t[0], "GroupThreshold") == 0)
                    tb->group_fold_threshold = atoi(s.t[1]);				/* For backward compatibility */
                else if (g_ascii_strcasecmp(s.t[0], "GroupFoldThreshold") == 0)
                    tb->group_fold_threshold = atoi(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "FoldThreshold") == 0)
                    tb->panel_fold_threshold = atoi(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "GroupBy") == 0)
                    tb->group_by = str2num(group_by_pair, s.t[1], tb->group_by);
                else if (g_ascii_strcasecmp(s.t[0], "ManualGrouping") == 0)
                    tb->manual_grouping = str2num(bool_pair, s.t[1], tb->manual_grouping);
                else if (g_ascii_strcasecmp(s.t[0], "UnfoldFocusedGroup") == 0)
                    tb->unfold_focused_group = str2num(bool_pair, s.t[1], tb->unfold_focused_group);
                else if (g_ascii_strcasecmp(s.t[0], "ShowSingleGroup") == 0)
                    tb->show_single_group = str2num(bool_pair, s.t[1], tb->show_single_group);
                else if (g_ascii_strcasecmp(s.t[0], "ShowIconsTitles") == 0)
                    tb->show_icons_titles = str2num(show_pair, s.t[1], tb->show_icons_titles);
                else if (g_ascii_strcasecmp(s.t[0], "ShowCloseButtons") == 0)
                    tb->show_close_buttons = str2num(bool_pair, s.t[1], tb->show_close_buttons);
                else if (g_ascii_strcasecmp(s.t[0], "SelfGroupSingleWindow") == 0)
                    tb->group_fold_threshold = str2num(bool_pair, s.t[1], 0) ? 1 : 2;        /* For backward compatibility */
                else if (g_ascii_strcasecmp(s.t[0], "SortBy1") == 0)
                    tb->sort_by[0] = str2num(sort_by_pair, s.t[1], tb->sort_by[0]);
                else if (g_ascii_strcasecmp(s.t[0], "SortBy2") == 0)
                    tb->sort_by[1] = str2num(sort_by_pair, s.t[1], tb->sort_by[1]);
                else if (g_ascii_strcasecmp(s.t[0], "SortBy3") == 0)
                    tb->sort_by[2] = str2num(sort_by_pair, s.t[1], tb->sort_by[2]);
                else if (g_ascii_strcasecmp(s.t[0], "SortReverse1") == 0)
                    tb->sort_reverse[0] = str2num(bool_pair, s.t[1], tb->sort_reverse[0]);
                else if (g_ascii_strcasecmp(s.t[0], "SortReverse2") == 0)
                    tb->sort_reverse[1] = str2num(bool_pair, s.t[1], tb->sort_reverse[1]);
                else if (g_ascii_strcasecmp(s.t[0], "SortReverse3") == 0)
                    tb->sort_reverse[2] = str2num(bool_pair, s.t[1], tb->sort_reverse[2]);
                else if (g_ascii_strcasecmp(s.t[0], "RearrangeTasks") == 0)
                    tb->rearrange = str2num(bool_pair, s.t[1], tb->rearrange);
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
                else if (g_ascii_strcasecmp(s.t[0], "MenuActionsTriggeredBy") == 0)
                    tb->menu_actions_click_press = str2num(action_trigged_by_pair, s.t[1], tb->menu_actions_click_press);
                else if (g_ascii_strcasecmp(s.t[0], "OtherActionsTriggeredBy") == 0)
                    tb->other_actions_click_press = str2num(action_trigged_by_pair, s.t[1], tb->other_actions_click_press);
                else if (g_ascii_strcasecmp(s.t[0], "HighlightModifiedTitles") == 0)
                    tb->highlight_modified_titles = str2num(bool_pair, s.t[1], tb->highlight_modified_titles);
                else if (g_ascii_strcasecmp(s.t[0], "UseGroupSeparators") == 0)
                    tb->use_group_separators = str2num(bool_pair, s.t[1], tb->use_group_separators);
                else if (g_ascii_strcasecmp(s.t[0], "GroupSeparatorSize") == 0)
                    tb->group_separator_size = atoi(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "UseXNetWmIconGeometry") == 0)
                    tb->use_x_net_wm_icon_geometry = str2num(bool_pair, s.t[1], tb->use_x_net_wm_icon_geometry);
                else if (g_ascii_strcasecmp(s.t[0], "UseXWindowPosition") == 0)
                    tb->use_x_window_position = str2num(bool_pair, s.t[1], tb->use_x_window_position);
                else if (g_ascii_strcasecmp(s.t[0], "MouseOverAction") == 0)
                    tb->mouse_over_action = str2num(mouse_over_action_pair, s.t[1], tb->mouse_over_action);
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
    TaskbarPlugin * tb = PRIV(p);

    if (tb->preview_panel_motion_timer != 0)
        g_source_remove(tb->preview_panel_motion_timer);

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
    g_signal_handlers_disconnect_by_func(gtk_widget_get_screen(plugin_widget(p)), taskbar_window_manager_changed, tb);

    if (tb->desktop_names != NULL)
        g_strfreev(tb->desktop_names);

    icon_grid_to_be_removed(tb->icon_grid);

    /* Deallocate task list. */
    while (tb->task_list != NULL)
        task_delete(tb, tb->task_list, TRUE);

    /* Deallocate class list. */
    while (tb->task_class_list != NULL)
    {
        TaskClass * tc = tb->task_class_list;
        tb->task_class_list = tc->task_class_flink;
        g_free(tc->class_name);
        g_free(tc);
    }

    if (tb->menu_config)
        g_strfreev(tb->menu_config);

    /* Deallocate other memory. */
    icon_grid_free(tb->icon_grid);
    gtk_widget_destroy(tb->menu);
    g_free(tb);
}

/* Callback from configuration dialog mechanism to apply the configuration. */
static void taskbar_apply_configuration(Plugin * p)
{
    TaskbarPlugin * tb = PRIV(p);

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
    const char* actions = _("|None|Show context menu|Close|Raise/Iconify|Iconify|Maximize|Shade|Undecorate|Fullscreen|Stick|Show window list (menu)|Show group window list (menu)|Next window|Previous window|Next window in current group|Previous window in current group|Next window in pointed group|Previous window in pointed group|Copy title");
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

    const char* sort_by = _("|Timestamp|Title|Focus (LRU)|State|Workspace");
    char* sort_by_1 = g_strdup_printf("%s%s", _("|First sort by"), sort_by);
    char* sort_by_2 = g_strdup_printf("%s%s", _("|Then sort by"), sort_by);
    char* sort_by_3 = g_strdup_printf("%s%s", _("|And last sort by"), sort_by);


    const char* click_press = _("|Click|Press");
    char* menu_actions_click_press = g_strdup_printf("%s%s", _("|Menu actions triggered by"), click_press);
    char* other_actions_click_press = g_strdup_printf("%s%s", _("|Other actions triggered by"), click_press);


    int min_width_max = 16;
    int max_width_max = 10000;
    int max_spacing = 50;
    int max_group_separator_size = 50;

    TaskbarPlugin * tb = PRIV(p);
    GtkWidget* dlg = create_generic_config_dlg(
        _(plugin_class(p)->name),
        GTK_WIDGET(parent),
    	    (GSourceFunc) taskbar_apply_configuration, (gpointer) p,
        _("Appearance"), (gpointer)NULL, (GType)CONF_TYPE_BEGIN_PAGE,

        _("|Show:|Icons only|Titles only|Icons and titles"), (gpointer)&tb->show_icons_titles, (GType)CONF_TYPE_ENUM,
        _("Show tooltips"), (gpointer)&tb->tooltips, (GType)CONF_TYPE_BOOL,
        _("Show close buttons"), (gpointer)&tb->show_close_buttons, (GType)CONF_TYPE_BOOL,
        _("Dimm iconified"), (gpointer)&tb->dimm_iconified, (GType)CONF_TYPE_BOOL,
        _("Display inactive buttons flat"), (gpointer)&tb->flat_inactive_buttons, (GType)CONF_TYPE_BOOL,
		_("Display active button flat"), (gpointer)&tb->flat_active_button, (GType)CONF_TYPE_BOOL,
        _("Bold font when mouse is over a button"), (gpointer)&tb->bold_font_on_mouse_over, (GType)CONF_TYPE_BOOL,
        _("Colorize buttons"), (gpointer)&tb->colorize_buttons, (GType)CONF_TYPE_BOOL,
        _("Display window thumbnails instead of icons (requires compositing wm enabled)"), (gpointer)&tb->use_thumbnails_as_icons, (GType)CONF_TYPE_BOOL,
        _("Highlight modified titles"), (gpointer)&tb->highlight_modified_titles, (GType)CONF_TYPE_BOOL,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("Maximum width of task button"), (gpointer)&tb->task_width_max, (GType)CONF_TYPE_INT,
        "int-min-value", (gpointer)&min_width_max, (GType)CONF_TYPE_SET_PROPERTY,
        "int-max-value", (gpointer)&max_width_max, (GType)CONF_TYPE_SET_PROPERTY,
        _("Spacing"), (gpointer)&tb->spacing, (GType)CONF_TYPE_INT,
        "int-max-value", (gpointer)&max_spacing, (GType)CONF_TYPE_SET_PROPERTY,
        _("Use group separators"), (gpointer)&tb->use_group_separators, (GType)CONF_TYPE_BOOL,
        _("Group separator size"), (gpointer)&tb->group_separator_size, (GType)CONF_TYPE_INT,
        "int-max-value", (gpointer)&max_group_separator_size, (GType)CONF_TYPE_SET_PROPERTY,
        "", 0, (GType)CONF_TYPE_END_TABLE,

        _("Behavior"), (gpointer)NULL, (GType)CONF_TYPE_BEGIN_PAGE,

        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("|Mode|Classic|Group windows|Show only active window"), (gpointer)&tb->mode, (GType)CONF_TYPE_ENUM,
        _("|Group by|None|Window class|Workspace|Window state"), (gpointer)&tb->group_by, (GType)CONF_TYPE_ENUM,
        //_("Group fold threshold"), (gpointer)&tb->group_fold_threshold, (GType)CONF_TYPE_INT,
        _("Fold group when it has Nr windows"), (gpointer)&tb->group_fold_threshold, (GType)CONF_TYPE_INT,
        _("Fold groups when taskbar has Nr windows"), (gpointer)&tb->panel_fold_threshold, (GType)CONF_TYPE_INT,
        _("Unfold focused group"), (gpointer)&tb->unfold_focused_group, (GType)CONF_TYPE_BOOL,
	_("Show only focused group"), (gpointer)&tb->show_single_group, (GType)CONF_TYPE_BOOL,
        _("Manual grouping"), (gpointer)&tb->manual_grouping, (GType)CONF_TYPE_BOOL,
        "", 0, (GType)CONF_TYPE_END_TABLE,

        _("Show iconified windows"), (gpointer)&tb->show_iconified, (GType)CONF_TYPE_BOOL,
        _("Show mapped windows"), (gpointer)&tb->show_mapped, (GType)CONF_TYPE_BOOL,
        _("Show windows from all desktops"), (gpointer)&tb->show_all_desks, (GType)CONF_TYPE_BOOL,

        _("Flash when there is any window requiring attention"), (gpointer)&tb->use_urgency_hint, (GType)CONF_TYPE_BOOL,

        _("Show windows from all desktops, if they requires attention"), (gpointer)&tb->show_urgency_all_desks, (GType)CONF_TYPE_BOOL,

        _("Sorting"), (gpointer)NULL, (GType)CONF_TYPE_BEGIN_PAGE,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        sort_by_1, (gpointer)&tb->sort_by[0], (GType)CONF_TYPE_ENUM,
        sort_by_2, (gpointer)&tb->sort_by[1], (GType)CONF_TYPE_ENUM,
        sort_by_3, (gpointer)&tb->sort_by[2], (GType)CONF_TYPE_ENUM,
        "", 0, (GType)CONF_TYPE_END_TABLE,
        _("Rearrange manually"), (gpointer)&tb->rearrange, (GType)CONF_TYPE_BOOL,

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

//        _("Response for mouse clicks or drag:"), (gpointer)NULL, (GType)CONF_TYPE_TITLE,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        menu_actions_click_press, (gpointer)&tb->menu_actions_click_press, (GType)CONF_TYPE_ENUM,
        other_actions_click_press, (gpointer)&tb->other_actions_click_press, (GType)CONF_TYPE_ENUM,
        "", 0, (GType)CONF_TYPE_END_TABLE,

//        _("Open group menu on mouse over"), (gpointer)&tb->open_group_menu_on_mouse_over, (GType)CONF_TYPE_BOOL,
        "|Mouse over|None|Show group menu|Show preview (compositing required)", (gpointer)&tb->mouse_over_action, (GType)CONF_TYPE_ENUM,

        _("Integration"), (gpointer)NULL, (GType)CONF_TYPE_BEGIN_PAGE,

        _("_NET_WM_ICON_GEOMETRY"), (gpointer)&tb->use_x_net_wm_icon_geometry, (GType)CONF_TYPE_BOOL,
        _("_LXPANEL_TASKBAR_WINDOW_POSITION"), (gpointer)&tb->use_x_window_position, (GType)CONF_TYPE_BOOL,

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
    g_free(sort_by_1);
    g_free(sort_by_2);
    g_free(sort_by_3);
    g_free(menu_actions_click_press);
    g_free(other_actions_click_press);

}

/* Save the configuration to the configuration file. */
static void taskbar_save_configuration(Plugin * p, FILE * fp)
{
    TaskbarPlugin * tb = PRIV(p);
    lxpanel_put_bool(fp, "tooltips", tb->tooltips);
    lxpanel_put_enum(fp, "ShowIconsTitles", tb->show_icons_titles, show_pair);
    lxpanel_put_str(fp, "FallbackIcon", tb->custom_fallback_icon);
    lxpanel_put_bool(fp, "ShowIconified", tb->show_iconified);
    lxpanel_put_bool(fp, "ShowMapped", tb->show_mapped);
    lxpanel_put_bool(fp, "ShowAllDesks", tb->show_all_desks);
    lxpanel_put_bool(fp, "ShowUrgencyAllDesks", tb->show_urgency_all_desks);
    lxpanel_put_bool(fp, "UseUrgencyHint", tb->use_urgency_hint);
    lxpanel_put_bool(fp, "FlatInactiveButtons", tb->flat_inactive_buttons);
    lxpanel_put_bool(fp, "FlatActiveButton", tb->flat_active_button);
    lxpanel_put_bool(fp, "BoldFontOnMouseOver", tb->bold_font_on_mouse_over);
    lxpanel_put_bool(fp, "ColorizeButtons", tb->colorize_buttons);
    lxpanel_put_bool(fp, "IconThumbnails", tb->use_thumbnails_as_icons);
    lxpanel_put_bool(fp, "DimmIconified", tb->dimm_iconified);
    lxpanel_put_int(fp, "MaxTaskWidth", tb->task_width_max);
    lxpanel_put_int(fp, "spacing", tb->spacing);
    lxpanel_put_enum(fp, "Mode", tb->mode, mode_pair);
    lxpanel_put_int(fp, "GroupFoldThreshold", tb->group_fold_threshold);
    lxpanel_put_int(fp, "FoldThreshold", tb->panel_fold_threshold);
    lxpanel_put_enum(fp, "GroupBy", tb->group_by, group_by_pair);
    lxpanel_put_bool(fp, "ManualGrouping", tb->manual_grouping);
    lxpanel_put_bool(fp, "UnfoldFocusedGroup", tb->unfold_focused_group);
    lxpanel_put_bool(fp, "ShowSingleGroup", tb->show_single_group);
    lxpanel_put_bool(fp, "ShowCloseButtons", tb->show_close_buttons);
    lxpanel_put_enum(fp, "SortBy1", tb->sort_by[0], sort_by_pair);
    lxpanel_put_enum(fp, "SortBy2", tb->sort_by[1], sort_by_pair);
    lxpanel_put_enum(fp, "SortBy3", tb->sort_by[2], sort_by_pair);
    lxpanel_put_bool(fp, "SortReverse1", tb->sort_reverse[0]);
    lxpanel_put_bool(fp, "SortReverse2", tb->sort_reverse[1]);
    lxpanel_put_bool(fp, "SortReverse3", tb->sort_reverse[2]);
    lxpanel_put_bool(fp, "RearrangeTasks", tb->rearrange);
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
    lxpanel_put_enum(fp, "MenuActionsTriggeredBy", tb->menu_actions_click_press, action_trigged_by_pair);
    lxpanel_put_enum(fp, "OtherActionsTriggeredBy", tb->other_actions_click_press, action_trigged_by_pair);
    //lxpanel_put_bool(fp, "OpenGroupMenuOnMouseOver", tb->open_group_menu_on_mouse_over);
    lxpanel_put_enum(fp, "MouseOverAction", tb->mouse_over_action, mouse_over_action_pair);
    lxpanel_put_bool(fp, "HighlightModifiedTitles", tb->highlight_modified_titles);
    lxpanel_put_bool(fp, "UseGroupSeparators", tb->use_group_separators);
    lxpanel_put_int(fp, "GroupSeparatorSize", tb->group_separator_size);
    lxpanel_put_bool(fp, "UseXNetWmIconGeometry", tb->use_x_net_wm_icon_geometry);
    lxpanel_put_bool(fp, "UseXWindowPosition", tb->use_x_window_position);
}

/* Callback when panel configuration changes. */
static void taskbar_panel_configuration_changed(Plugin * p)
{
    TaskbarPlugin * tb = PRIV(p);
    taskbar_update_style(tb);
    taskbar_make_menu(tb);
    GtkOrientation bo = (plugin_get_orientation(p) == ORIENT_HORIZ) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    icon_grid_set_expand(tb->icon_grid, taskbar_task_button_is_expandable(tb));
    icon_grid_set_geometry(tb->icon_grid, bo,
        taskbar_get_task_button_max_width(tb), plugin_get_icon_size(p) + BUTTON_HEIGHT_EXTRA,
        tb->spacing, 0, panel_get_oriented_height_pixels(plugin_panel(p)));

    /* If the icon size changed, refetch all the icons. */
    if (plugin_get_icon_size(p) != tb->icon_size)
    {
        tb->icon_size = plugin_get_icon_size(p);
        Task * tk;
        for (tk = tb->task_list; tk != NULL; tk = tk->task_flink)
        {
            task_update_icon(tk, None, TRUE);
        }
    }

    /* Redraw all the labels.  Icon size or font color may have changed. */
    taskbar_redraw(tb);
}

/******************************************************************************/


static void taskbar_run_command(Plugin * p, char ** argv, int argc)
{
    if (argc < 1)
        return;

    TaskbarPlugin * tb = PRIV(p);

    int action = str2num(action_pair, argv[0], -1);
    if (action > 0)
    {
        Task * tk = tb->focused;
        if (tk)
            task_action(tk, action, NULL, tk, FALSE);
    }

}

/******************************************************************************/

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
    panel_configuration_changed : taskbar_panel_configuration_changed,
    run_command : taskbar_run_command
};
