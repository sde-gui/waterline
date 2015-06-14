/**
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
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <gdk/gdkx.h>

#include <sde-utils.h>

#include <waterline/global.h>
#include "plugin_internal.h"
#include "plugin_private.h"
#include <waterline/paths.h>
#include <waterline/panel.h>
#include "wtl_private.h"
#include "panel_internal.h"
#include "panel_private.h"
#include <waterline/misc.h>
#include "bg.h"
#include <waterline/Xsupport.h>
#include <waterline/gtkcompat.h>

/******************************************************************************/

#define PANEL_ICON_SIZE               24	/* Default size of panel icons */
#define PANEL_HEIGHT_DEFAULT          26	/* Default height of horizontal panel */

/******************************************************************************/

/* forward declarations */

static void panel_destroy(Panel *p);
static int panel_start(Panel *p, const char * configuration, const char * source);
static void panel_start_gui(Panel *p);
static void panel_size_position_changed(Panel *p, gboolean position_changed);
static void panel_calculate_position(Panel *p);

extern void update_panel_geometry(Panel* p);

static void panel_notify_plugins_on_configuration_change(Panel * p);
static void panel_notify_plugins_on_compositing_mode_change(Panel * p);

/******************************************************************************/

/* Globals */

static gchar version[] = VERSION;
gchar *cprofile = "default"; /* used in path.c */
static gchar *force_colormap = "rgba";

gboolean quit_in_menu = FALSE;
static GtkWindowGroup* window_group; /* window group used to limit the scope of model dialog. */

FbEv *fbev = NULL;

static GSList* all_panels = NULL;  /* a single-linked list storing all panels */

gboolean is_restarting = FALSE;

gchar * _wtl_agent_id = NULL;

/******************************************************************************/

static gboolean force_compositing_wm_disabled = FALSE;
static gboolean force_composite_disabled = FALSE;

/******************************************************************************/

const char * wtl_license = "This program is free software; you can redistribute it and/or\nmodify it under the terms of the GNU General Public License\nas published by the Free Software Foundation; either version 2\nof the License, or (at your option) any later version.\n\nThis program is distributed in the hope that it will be useful,\nbut WITHOUT ANY WARRANTY; without even the implied warranty of\nMERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\nGNU General Public License for more details.\n";

const char * wtl_website = "http://dev.make-linux.org/projects/waterline";
const char * wtl_email = "igeekless@gmail.com";
const char * wtl_bugreporting = "http://dev.make-linux.org/projects/waterline/issues";

/******************************************************************************/

const char* wtl_agent_id(void)
{
    return _wtl_agent_id;
}

/******************************************************************************/

/* A hack used to be compatible with Gnome panel for gtk+ themes.
 * Some gtk+ themes define special styles for desktop panels.
 * http://live.gnome.org/GnomeArt/Tutorials/GtkThemes/GnomePanel
 * So we make a derived class from GtkWindow named PanelToplevel
 * and create the panels with it to be compatible with Gnome themes.
 */
#define PANEL_TOPLEVEL_TYPE				(panel_toplevel_get_type())

typedef struct _PanelToplevel			PanelToplevel;
typedef struct _PanelToplevelClass		PanelToplevelClass;
struct _PanelToplevel
{
	GtkWindow parent;
};
struct _PanelToplevelClass
{
	GtkWindowClass parent_class;
};
G_DEFINE_TYPE(PanelToplevel, panel_toplevel, GTK_TYPE_WINDOW);
static void panel_toplevel_class_init(PanelToplevelClass *klass)
{
}
static void panel_toplevel_init(PanelToplevel *self)
{
}

/******************************************************************************/

/*= getters =*/

GtkStyle * panel_get_default_style(Panel * p)
{
    return p->defstyle;
}

GtkWidget * panel_get_toplevel_widget(Panel * p)
{
    return p->topgwin;
}

GdkWindow * panel_get_toplevel_window(Panel * p)
{
    return p->topgwin->window;
}

Window      panel_get_toplevel_xwindow(Panel * p)
{
    return p->topxwin;
}

GdkColormap * panel_get_color_map(Panel * p)
{
    return gdk_drawable_get_colormap(panel_get_toplevel_window(p));
}

int panel_get_edge(Panel * p)
{
    return p->edge;
}

int panel_get_orientation(Panel * p)
{
    return p->orientation;
}

int panel_get_oriented_height_pixels(Panel * p)
{
    return p->oriented_height;
}

int panel_get_icon_size(Panel * p)
{
    int max_icon_size = p->oriented_height;
    return (p->preferred_icon_size < max_icon_size) ? p->preferred_icon_size : max_icon_size;
}

int panel_get_available_screen_width(Panel * p)
{
    switch (p->edge)
    {
        case EDGE_TOP:
        case EDGE_BOTTOM:
            return gdk_screen_height();
        case EDGE_LEFT:
            return gdk_screen_width() - (p->cx + p->cw);
        case EDGE_RIGHT:
            return gdk_screen_width() - p->cw;
    }
    return 0;
}

int panel_get_available_screen_height(Panel * p)
{
    switch (p->edge)
    {
        case EDGE_TOP:
            return gdk_screen_height() - (p->cy + p->ch);
        case EDGE_BOTTOM:
            return gdk_screen_height() - p->ch;
        case EDGE_LEFT:
        case EDGE_RIGHT:
            return gdk_screen_width();
    }
    return 0;
}

/******************************************************************************/

/* Allocate and initialize new Panel structure. */
static Panel* panel_allocate(void)
{
    Panel* p = g_new0(Panel, 1);
    p->align = ALIGN_CENTER;
    p->edge = EDGE_NONE;
    p->oriented_width_type = WIDTH_PERCENT;
    p->oriented_width = 100;
    p->oriented_height_type = HEIGHT_PIXEL;
    p->oriented_height = PANEL_HEIGHT_DEFAULT;
    p->output_target = OUTPUT_PRIMARY_MONITOR;
    p->set_strut = 1;
    p->autohide_visible = TRUE;
    p->gobelow = FALSE;
    p->visible = TRUE;
    p->height_when_hidden = 1;
    p->background_mode = BACKGROUND_SYSTEM;
    p->alpha = 255;
    gdk_color_parse("white", &p->background_color);
    p->use_font_color = 0;
    gdk_color_parse("black", &p->font_color);
    p->use_font_size = 0;
    p->font_size = 10;
    p->spacing = 0;
    p->preferred_icon_size = PANEL_ICON_SIZE;
    p->visibility_mode = VISIBILITY_ALWAYS;

    p->json = json_object();

    return p;
}

/* Normalize panel configuration after load from file or reconfiguration. */
static void panel_normalize_configuration(Panel* p)
{
    panel_set_panel_configuration_changed( p );
    if (p->oriented_width < 0)
        p->oriented_width = 100;
    if (p->oriented_width_type == WIDTH_PERCENT && p->oriented_width > 100)
        p->oriented_width = 100;
    p->oriented_height_type = HEIGHT_PIXEL;
    if (p->oriented_height_type == HEIGHT_PIXEL) {
        if (p->oriented_height < PANEL_HEIGHT_MIN)
            p->oriented_height = PANEL_HEIGHT_MIN;
        else if (p->oriented_height > PANEL_HEIGHT_MAX)
            p->oriented_height = PANEL_HEIGHT_MAX;
    }
}

/******************************************************************************/

/*= wm properties =*/


static void panel_set_desktop_icon_overlap_mode(Panel * panel)
{
    guint32 val[4];
    int val_size = 0;
    if (panel->visibility_mode != VISIBILITY_AUTOHIDE)
    {
        val[0] = panel->cx;
        val[1] = panel->cy;
        val[2] = panel->cw;
        val[3] = panel->ch;
        val_size = 4;
    }
    else
    {
        val[0] = 0;
        val_size = 1;
    }
    XChangeProperty(GDK_DISPLAY(), panel->topxwin, a_SDE_DONT_OVERLAP_DESKTOP_ICONS, XA_CARDINAL, 32,
          PropModeReplace, (unsigned char *) val, val_size);
}

static gboolean panel_set_wm_strut_real(Panel *p)
{
    p->set_wm_strut_idle = 0;

    int index;
    gulong strut_size;
    gulong strut_lower;
    gulong strut_upper;

    /* Dispatch on edge to set up strut parameters. */
    switch (p->edge)
    {
        case EDGE_LEFT:
            index = 0;
            strut_size = p->cw;
            strut_lower = p->cy;
            strut_upper = p->cy + p->ch - 1;
            break;
        case EDGE_RIGHT:
            index = 1;
            strut_size = p->cw;
            strut_lower = p->cy;
            strut_upper = p->cy + p->ch - 1;
            break;
        case EDGE_TOP:
            index = 2;
            strut_size = p->ch;
            strut_lower = p->cx;
            strut_upper = p->cx + p->cw - 1;
            break;
        case EDGE_BOTTOM:
            index = 3;
            strut_size = p->ch;
            strut_lower = p->cx;
            strut_upper = p->cx + p->cw - 1;
            break;
        default:
            return FALSE;
    }

    strut_size += p->edge_margin;

    /* Handle autohide case.  EWMH recommends having the strut be the minimized size. */
    if (p->visibility_mode == VISIBILITY_AUTOHIDE || p->visibility_mode == VISIBILITY_GOBELOW)
    {
        strut_size = p->height_when_hidden;
    }
    else if (!p->set_strut)
    {
        strut_size = 0;
        strut_lower = 0;
        strut_upper = 0;
    }

    /* If strut value changed, set the property value on the panel window.
     * This avoids property change traffic when the panel layout is recalculated but strut geometry hasn't changed. */
    if ((gtk_widget_get_mapped(p->topgwin))
    && ((p->strut_size != strut_size) ||
        (p->strut_lower != strut_lower) ||
        (p->strut_upper != strut_upper) ||
        (p->strut_edge != p->edge)))
    {
        p->strut_size = strut_size;
        p->strut_lower = strut_lower;
        p->strut_upper = strut_upper;
        p->strut_edge = p->edge;

        /* If window manager supports STRUT_PARTIAL, it will ignore STRUT.
         * Set STRUT also for window managers that do not support STRUT_PARTIAL. */
        if (strut_size != 0)
        {
            /* Set up strut value in property format. */
            gulong desired_strut[12];
            memset(desired_strut, 0, sizeof(desired_strut));
            desired_strut[index] = strut_size;
            desired_strut[4 + index * 2] = strut_lower;
            desired_strut[5 + index * 2] = strut_upper;

            XChangeProperty(GDK_DISPLAY(), p->topxwin, a_NET_WM_STRUT_PARTIAL,
                XA_CARDINAL, 32, PropModeReplace,  (unsigned char *) desired_strut, 12);
            XChangeProperty(GDK_DISPLAY(), p->topxwin, a_NET_WM_STRUT,
                XA_CARDINAL, 32, PropModeReplace,  (unsigned char *) desired_strut, 4);
        }
        else
        {
            XDeleteProperty(GDK_DISPLAY(), p->topxwin, a_NET_WM_STRUT);
            XDeleteProperty(GDK_DISPLAY(), p->topxwin, a_NET_WM_STRUT_PARTIAL);
        }
    }

    return FALSE;
}

static void panel_set_wm_strut(Panel *p)
{
    if (p->set_wm_strut_idle == 0)
        p->set_wm_strut_idle = g_idle_add_full( G_PRIORITY_LOW,
            (GSourceFunc)panel_set_wm_strut_real, p, NULL );
}

static void panel_set_dock_type(Panel *p)
{
    Atom state = a_NET_WM_WINDOW_TYPE_DOCK;
    XChangeProperty(GDK_DISPLAY(), p->topxwin,
                    a_NET_WM_WINDOW_TYPE, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *) &state, 1);
}

static void panel_set_wm_state(Panel *p)
{
    gboolean below = (p->visibility_mode == VISIBILITY_BELOW) || p->gobelow;

    GdkWindow * w = gtk_widget_get_window(p->topgwin);

    if (below)
    {
        gdk_window_set_keep_below(w, TRUE);
        gdk_window_lower(w);
    }
    else
    {
        gdk_window_set_keep_below(w, FALSE);
        gdk_window_raise(w);
    }

}

/******************************************************************************/

/*= autohide tracking =*/

static void panel_set_autohide_visibility(Panel *p, gboolean visible)
{
    gboolean autohide_visible = visible;
    gboolean gobelow = !visible;

    if (p->visibility_mode != VISIBILITY_AUTOHIDE)
        autohide_visible = TRUE;

    if (p->visibility_mode != VISIBILITY_GOBELOW)
        gobelow = FALSE;

    if (p->autohide_visible != autohide_visible)
    {
        p->autohide_visible = autohide_visible;

        if (!autohide_visible)
            gtk_widget_hide(p->plugin_box);

        panel_calculate_position(p);
        gtk_widget_set_size_request(p->topgwin, p->aw, p->ah);
        gdk_window_move(p->topgwin->window, p->ax, p->ay);

        if (autohide_visible)
            gtk_widget_show(p->plugin_box);

        panel_set_wm_strut(p);
    }

    if (p->gobelow != gobelow)
    {
        p->gobelow = gobelow;
        panel_set_wm_state(p);
    }
}

static gboolean panel_leave_real(Panel *p);

void panel_autohide_conditions_changed( Panel* p )
{
    gboolean autohide_visible = FALSE;

    if (p->visibility_mode != VISIBILITY_AUTOHIDE && p->visibility_mode != VISIBILITY_GOBELOW)
        autohide_visible = TRUE;

    if (!autohide_visible)
    {
        /* If the pointer is grabbed by this application, leave the panel displayed.
         * There is no way to determine if it is grabbed by another application,
         * such as an application that has a systray icon. */
        if (gdk_display_pointer_is_grabbed(p->display))
            autohide_visible = TRUE;
    }

    /* Visibility can be locked by plugin. */
    if (!autohide_visible)
    {
        GList * l;
        for (l = p->plugins; l != NULL; l = l->next)
        {
            Plugin * pl = (Plugin *) l->data;
            if (pl->lock_visible)
            {
                autohide_visible = TRUE;
                break;
            }
        }
    }

    if (!autohide_visible)
    {
        gint x, y;
        gdk_display_get_pointer(p->display, NULL, &x, &y, NULL);
        if ((p->cx <= x) && (x <= (p->cx + p->cw)) && (p->cy <= y) && (y <= (p->cy + p->ch)))
        {
            autohide_visible = TRUE;
        }
    }

    if (autohide_visible)
    {
        panel_set_autohide_visibility(p, TRUE);
        if (p->visibility_mode == VISIBILITY_AUTOHIDE || p->visibility_mode == VISIBILITY_GOBELOW)
        {
            if (p->hide_timeout == 0)
                p->hide_timeout = g_timeout_add(500, (GSourceFunc) panel_leave_real, p);
        }
    }
    else
    {
        panel_set_autohide_visibility(p, FALSE);
        if (p->hide_timeout)
        {
            g_source_remove(p->hide_timeout);
            p->hide_timeout = 0;
        }
    }
}

static gboolean panel_leave_real(Panel *p)
{
    panel_autohide_conditions_changed(p);
    return TRUE;
}

static gboolean panel_enter(GtkImage *widget, GdkEventCrossing *event, Panel *p)
{
    panel_autohide_conditions_changed(p);
    return FALSE;
}

static gboolean panel_drag_motion(GtkWidget *widget, GdkDragContext *drag_context, gint x,
      gint y, guint time, Panel *p)
{
    panel_autohide_conditions_changed(p);
    return TRUE;
}

static void panel_establish_autohide(Panel *p)
{
    if (p->visibility_mode == VISIBILITY_AUTOHIDE || p->visibility_mode == VISIBILITY_GOBELOW)
    {
        gtk_widget_add_events(p->topgwin, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
        g_signal_connect(G_OBJECT(p->topgwin), "enter-notify-event", G_CALLBACK(panel_enter), p);
        g_signal_connect(G_OBJECT(p->topgwin), "drag-motion", (GCallback) panel_drag_motion, p);
        gtk_drag_dest_set(p->topgwin, GTK_DEST_DEFAULT_MOTION, NULL, 0, 0);
        gtk_drag_dest_set_track_motion(p->topgwin, TRUE);
    }
    panel_autohide_conditions_changed(p);
}

/******************************************************************************/

void panel_application_class_visibility_changed(Panel* p)
{
    GList * l;
    for (l = p->plugins; l; l = l->next)
    {
        Plugin * pl = (Plugin *) l->data;
        if (pl->class->application_class_visibility_changed)
        {
            pl->class->application_class_visibility_changed(pl);
        }
    }
    return;
}

gboolean panel_is_application_class_visible(Panel* p, const char * class_name)
{
    GList * l;
    for (l = p->plugins; l; l = l->next)
    {
        Plugin * pl = (Plugin *) l->data;
        if (pl->class->is_application_class_visible)
        {
            if (pl->class->is_application_class_visible(pl, class_name))
                return TRUE;
        }
    }
    return FALSE;
}

/******************************************************************************/

int panel_count(void)
{
    int result = 0;
    GSList * l;
    for (l = all_panels; l; l = l->next)
    {
        result++;
    }
    return result;
}

/******************************************************************************/

/*= command handling =*/

Plugin * panel_get_plugin_by_name(Panel* p, const gchar * name)
{
    GList * l;

    /* First trying to match plugin name as is. */
    for (l = p->plugins; l; l = l->next)
    {
        Plugin * pl = (Plugin *) l->data;
        if (!strcmp(pl->class->type, name))
            return pl;
    }

    /* Now trying to match plugin name as "name-index" */

    int len = strlen(name);
    if (len == 0)
        return NULL;

    int i = len - 1;
    while (name[i] >= '0' && name[i] <= '9' && i >= 0)
        i--;

    if (i == 0 || i == len - 1)
        return NULL;

    int index = atoi(name + i + 1);
    if (index == 0)
        return NULL;

    gchar * prefix = g_strdup(name);
    prefix[i + 1] = 0;
    if (prefix[i] == '-')
        prefix[i] = 0;

    int current_index = 0;
    for (l = p->plugins; l; l = l->next)
    {
        Plugin * pl = (Plugin *) l->data;
        if (!strcmp(pl->class->type, prefix))
        {
            current_index++;
            if (current_index == index)
            {
                g_free(prefix);
                return pl;
            }
        }
    }

    g_free(prefix);

    return NULL;
}

static Panel * panel_get_by_name(gchar * name)
{
    GSList * l;
    for (l = all_panels; l; l = l->next)
    {
        Panel * p = (Panel *) l->data;
        if (strcmp(p->name, name) == 0)
            return p;
    }
    return NULL;
}

static void cmd_panel_visible(Panel * panel, char ** argv, int argc)
{
    gboolean visible = !panel->visible;

    if (argc > 1)
    {
        if (strcmp(argv[1], "true") == 0 || strcmp(argv[1], "1") == 0)
            visible = TRUE;
        else if (strcmp(argv[1], "false") == 0 || strcmp(argv[1], "0") == 0)
            visible = FALSE;
    }

    if (visible != panel->visible)
    {
        panel->visible = visible;
        gtk_widget_set_visible(panel->topgwin, visible);
        panel_set_wm_strut(panel);
        panel_size_position_changed(panel, TRUE);
        if (visible)
        {
            /* send it to running wm */
            Xclimsg(panel->topxwin, a_NET_WM_DESKTOP, 0xFFFFFFFF, 0, 0, 0, 0);
            /* and assign it ourself just for case when wm is not running */
            guint32 val = 0xFFFFFFFF;
            XChangeProperty(GDK_DISPLAY(), panel->topxwin, a_NET_WM_DESKTOP, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *) &val, 1);

            panel_set_wm_state(panel);
        }
    }
}

static void cmd_panel_autohide(Panel * panel, char ** argv, int argc)
{
    gboolean autohide_old_value = (panel->visibility_mode == VISIBILITY_AUTOHIDE);
    gboolean autohide = !autohide_old_value;

    if (argc > 1)
    {
        if (strcmp(argv[1], "true") == 0 || strcmp(argv[1], "1") == 0)
            autohide = TRUE;
        else if (strcmp(argv[1], "false") == 0 || strcmp(argv[1], "0") == 0)
            autohide = FALSE;
    }

    if (autohide != autohide_old_value)
    {
        panel->visibility_mode = autohide ? VISIBILITY_AUTOHIDE : VISIBILITY_ALWAYS;
        update_panel_geometry(panel);
    }
}

static void cmd_panel(Panel * panel, char ** argv, int argc)
{
    if (!panel)
        return;

    if (argc < 1)
        return;

    if (strcmp(argv[0], "visible") == 0)
        cmd_panel_visible(panel, argv, argc);
    else if (strcmp(argv[0], "autohide") == 0)
        cmd_panel_autohide(panel, argv, argc);
    else if (strcmp(argv[0], "plugin") == 0 && argc >= 3)
    {
        Plugin * pl = panel_get_plugin_by_name(panel, argv[1]);
        if (pl)
        {
            plugin_run_command(pl, argv + 2, argc - 2);
        }
    }
}


static void cmd_run(char ** argv, int argc)
{
#ifndef DISABLE_MENU
        wtl_show_run_box();
#endif
}

static gboolean show_system_menu()
{
    GSList * l1;
    for (l1 = all_panels; l1; l1 = l1->next)
    {
        Panel * p = (Panel *) l1->data;

        GList * l2;
        for (l2 = p->plugins; l2; l2 = l2->next)
        {
            Plugin * pl = (Plugin *) l2->data;
            if (pl->has_system_menu && pl->class->open_system_menu)
            {
                pl->class->open_system_menu(pl);
                return FALSE;
            }
        }
    }
    return FALSE;
}

static void cmd_menu(char ** argv, int argc)
{
    /* FIXME: I've no idea why this doesn't work without timeout
       under some WMs, like icewm. */
    g_timeout_add( 200, (GSourceFunc)show_system_menu, NULL );
}

static void cmd_config(char ** argv, int argc)
{
    Panel * p = ((all_panels != NULL) ? all_panels->data : NULL);
    if (p != NULL)
        panel_configure(p, 0);
}

static void cmd_restart(char ** argv, int argc)
{
    wtl_restart();
}

static void cmd_exit(char ** argv, int argc)
{
    gtk_main_quit();
}

static void process_command(char ** argv, int argc)
{
    //g_print("%s\n", argv[0]);

    if (argc < 1)
        return;

    if (strcmp(argv[0], "panel") == 0 && argc > 1)
    {
        Panel * p = panel_get_by_name(argv[1]);
        cmd_panel(p, argv + 2, argc - 2);
    }
    else if (strcmp(argv[0], "run") == 0)
        cmd_run(argv + 1, argc - 1);
    else if (strcmp(argv[0], "menu") == 0)
        cmd_menu(argv + 1, argc - 1);
    else if (strcmp(argv[0], "config") == 0)
        cmd_config(argv + 1, argc - 1);
    else if (strcmp(argv[0], "restart") == 0)
        cmd_restart(argv + 1, argc - 1);
    else if (strcmp(argv[0], "exit") == 0)
        cmd_exit(argv + 1, argc - 1);
    else if (strcmp(argv[0], "glib_mem_profiler") == 0)
        g_mem_profile();
}

/******************************************************************************/

/*= panel's handlers for WM events =*/

static GdkFilterReturn panel_event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer not_used)
{
    Atom at;
    Window win;
    XEvent *ev = (XEvent *) xevent;

    if (ev->type != PropertyNotify )
    {
        if( ev->type == DestroyNotify )
        {
            fb_ev_emit_destroy( fbev, ((XDestroyWindowEvent*)ev)->window );
        }
        return GDK_FILTER_CONTINUE;
    }

    at = ev->xproperty.atom;
    win = ev->xproperty.window;
    if (win == GDK_ROOT_WINDOW())
    {
        su_log_debug2("PropertyNotify: atom = 0x%x", at);

        if (at == a_NET_CLIENT_LIST)
        {
            fb_ev_emit(fbev, EV_CLIENT_LIST);
        }
        else if (at == a_NET_CURRENT_DESKTOP)
        {
            GSList* l;
            for( l = all_panels; l; l = l->next )
                ((Panel*)l->data)->curdesk = get_net_current_desktop();
            fb_ev_emit(fbev, EV_CURRENT_DESKTOP);
        }
        else if (at == a_NET_NUMBER_OF_DESKTOPS)
        {
            GSList* l;
            for( l = all_panels; l; l = l->next )
                ((Panel*)l->data)->desknum = get_net_number_of_desktops();
            fb_ev_emit(fbev, EV_NUMBER_OF_DESKTOPS);
        }
        else if (at == a_NET_DESKTOP_NAMES)
        {
            fb_ev_emit(fbev, EV_DESKTOP_NAMES);
        }
        else if (at == a_NET_ACTIVE_WINDOW)
        {
            fb_ev_emit(fbev, EV_ACTIVE_WINDOW );
        }
        else if (at == a_NET_CLIENT_LIST_STACKING)
        {
            fb_ev_emit(fbev, EV_CLIENT_LIST_STACKING);
        }
        else if (at == a_XROOTPMAP_ID)
        {
            GSList* l;
            for( l = all_panels; l; l = l->next )
            {
                Panel* p = (Panel*)l->data;
                if (p->bg)
                    fb_bg_notify_changed_bg(p->bg);
            }
        }
        else if (at == a_NET_WORKAREA)
        {
            GSList* l;
            for( l = all_panels; l; l = l->next )
            {
                Panel* p = (Panel*)l->data;
                g_free( p->workarea );
                p->workarea = wtl_x11_get_xa_property (GDK_ROOT_WINDOW(), a_NET_WORKAREA, XA_CARDINAL, &p->wa_len);
                /* print_wmdata(p); */
            }
        }
        else if (at == a_WATERLINE_TEXT_CMD)
        {
            int remote_command_argc = 0;;
            char ** remote_command_argv = NULL;
            remote_command_argv = wtl_x11_get_utf8_property_list(GDK_ROOT_WINDOW(), a_WATERLINE_TEXT_CMD, &remote_command_argc);
            if (remote_command_argc > 0 && remote_command_argv)
            {
                unsigned char b[1];
                XChangeProperty (GDK_DISPLAY(), GDK_ROOT_WINDOW(), a_WATERLINE_TEXT_CMD, XA_STRING, 8, PropModeReplace, b, 0);
                process_command(remote_command_argv, remote_command_argc);
            }
            g_strfreev(remote_command_argv);
        }
        else if (at == a_NET_SUPPORTED)
        {
            update_net_supported();
        }
        else
            return GDK_FILTER_CONTINUE;

        return GDK_FILTER_REMOVE;
    }
    return GDK_FILTER_CONTINUE;
}

/******************************************************************************/

static gboolean panel_expose_event(GtkWidget *widget, GdkEventExpose *event, Panel *p)
{
    cairo_t *cr;

    //su_log_debug2("panel expose");

    cr = gdk_cairo_create(widget->window); /* create cairo context */

    float a = (float) p->alpha / 255;
    float r = (float) p->background_color.red / 65535;
    float g = (float) p->background_color.green / 65535;
    float b = (float) p->background_color.blue / 65535;

    if (p->background_pixmap)
    {
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(cr);

        gdk_cairo_set_source_pixmap(cr, p->background_pixmap, 0, 0);
        cairo_pattern_t * pattern = cairo_get_source(cr);
        cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);

        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_paint_with_alpha(cr, a);

/*
        cairo_pattern_t * pattern = gdk_window_get_background_pattern(widget->window);
        cairo_set_source(cr, pattern);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_paint_with_alpha(cr, a);
*/
    }
    else if (p->background_mode == BACKGROUND_COLOR)
    {
        cairo_set_source_rgba(cr, r, g, b, a);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(cr);
    }
    else
    {
        GtkStyle * style = gtk_widget_get_style(widget);

        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(cr);

        if (style->bg_pixmap[GTK_STATE_NORMAL])
        {
            gdk_cairo_set_source_pixmap(cr, style->bg_pixmap[GTK_STATE_NORMAL], 0, 0);
            cairo_pattern_t * pattern = cairo_get_source(cr);
            cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);

            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            cairo_paint_with_alpha(cr, a);
        }
        else
        {
            gdk_cairo_set_source_color(cr, &style->bg[GTK_STATE_NORMAL]);
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            cairo_paint_with_alpha(cr, a);
        }

    }

    cairo_destroy(cr);

    return FALSE;
}

/******************************************************************************/

void panel_apply_icon( GtkWindow *w )
{
    if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), "start-here"))
    {
        gtk_window_set_icon(w,
            gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "start-here", 24, 0, NULL));
    }
    else
    {
        gchar * icon_path = wtl_resolve_own_resource("", "images", "my-computer.png", 0);
        gtk_window_set_icon_from_file(w, icon_path, NULL);
        g_free(icon_path);
    }
}

/******************************************************************************/

gboolean panel_is_composited(Panel * p)
{
    return !force_compositing_wm_disabled && panel_is_composite_available(p) && gtk_widget_is_composited(p->topgwin);
}

gboolean panel_is_composite_available(Panel * p)
{
    return !force_composite_disabled && is_xcomposite_available();
}

/******************************************************************************/

/*= panel's handlers for GTK events =*/

static gint panel_delete_event(GtkWidget * widget, GdkEvent * event, gpointer data)
{
    return FALSE;
}

static gint panel_destroy_event(GtkWidget * widget, GdkEvent * event, gpointer data)
{
    //Panel *p = (Panel *) data;
    //if (!p->self_destroy)
    gtk_main_quit();
    return FALSE;
}

static void on_root_bg_changed(FbBg *bg, Panel* p)
{
    panel_update_background( p );
}

void panel_determine_background_pixmap(Panel * p, GtkWidget * widget, GdkWindow * window)
{
    GdkPixmap * pixmap = NULL;

    p->rgba_transparency = (p->alpha < 255) && panel_is_composited(p);

    /* Free p->bg if it is not going to be used. */
    if ((p->background_mode != BACKGROUND_IMAGE) && (p->bg != NULL))
    {
        g_signal_handlers_disconnect_by_func(G_OBJECT(p->bg), on_root_bg_changed, p);
        g_object_unref(p->bg);
        p->bg = NULL;
    }

    if (p->rgba_transparency)
    {
        if (!p->expose_event_connected && widget == p->topgwin)
        {
            g_signal_connect(G_OBJECT(p->topgwin), "expose_event", G_CALLBACK(panel_expose_event), p);
            su_log_debug("panel_expose_event connected");
            p->expose_event_connected = TRUE;
        }
    }
    else
    {
        if (p->expose_event_connected && widget == p->topgwin)
        {
            g_signal_handlers_disconnect_by_func(G_OBJECT(p->topgwin), G_CALLBACK(panel_expose_event), p);
            su_log_debug("panel_expose_event disconnected");
            p->expose_event_connected = FALSE;
        }
    }

    if (p->background_mode == BACKGROUND_IMAGE)
    {
        /* User specified background pixmap. */
        if (p->background_file != NULL)
            pixmap = fb_bg_get_pix_from_file(widget, p->background_file);
    }
    else if (p->background_mode == BACKGROUND_COLOR && !p->rgba_transparency)
    {
        /* Transparent.  Determine the appropriate value from the root pixmap. */
        if (p->bg == NULL)
        {
            p->bg = fb_bg_get_for_display();
            g_signal_connect(G_OBJECT(p->bg), "changed", G_CALLBACK(on_root_bg_changed), p);
        }
        pixmap = fb_bg_get_xroot_pix_for_win(p->bg, widget);
        if ((pixmap != NULL) && (pixmap != GDK_NO_BG) && (p->alpha != 0))
            fb_bg_composite(pixmap, widget->style->black_gc, gcolor2rgb24(&p->background_color), p->alpha);
    }
    else if (p->background_mode == BACKGROUND_SYSTEM)
    {
        if (p->stretch_background)
        {
            GtkStyle * style = gtk_widget_get_style(widget);
            if (style && style->bg_pixmap[GTK_STATE_NORMAL])
                pixmap = g_object_ref(style->bg_pixmap[GTK_STATE_NORMAL]);
        }
    }

    if (p->background_pixmap && widget == p->topgwin)
    {
        g_object_unref(G_OBJECT(p->background_pixmap));
        p->background_pixmap = NULL;
    }


    if (pixmap && p->stretch_background && widget == p->topgwin)
    {
         gint pixmap_width, pixmap_height;
         gint window_width, window_height;
         gdk_drawable_get_size(pixmap, &pixmap_width, &pixmap_height);
         gdk_drawable_get_size(widget->window, &window_width, &window_height);
         if (pixmap_width != window_width && pixmap_height != window_height)
         {
             GdkPixbuf * pixbuf1 = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, 0, 0, 0, 0, pixmap_width, pixmap_height);
             GdkPixbuf * pixbuf2 = gdk_pixbuf_scale_simple(pixbuf1, window_width, window_height, GDK_INTERP_HYPER);
             GdkPixmap * pixmap2 = gdk_pixmap_new(pixmap, window_width, window_height, -1);
             gdk_draw_pixbuf(pixmap2, widget->style->black_gc, pixbuf2, 0, 0, 0, 0, window_width, window_height, GDK_RGB_DITHER_NONE, 0, 0);
             g_object_unref(pixbuf1);
             g_object_unref(pixbuf2);
             g_object_unref(pixmap);
             pixmap = pixmap2;
         }
    }

    if (p->rgba_transparency)
    {
        if (widget == p->topgwin)
            p->background_pixmap = pixmap;
        else if (pixmap)
            g_object_unref(pixmap);

        gtk_widget_set_app_paintable(widget, TRUE);
        gdk_window_set_back_pixmap(window, NULL, FALSE);
    }
    else
    {
        gdk_window_set_back_pixmap(window, pixmap, FALSE);
        gtk_widget_set_app_paintable(widget, pixmap != NULL);

        if (pixmap != NULL)
            g_object_unref(pixmap);
    }
}

/* Update the background of the entire panel.
 * This function should only be called after the panel has been realized. */
void panel_update_background(Panel * p)
{
    if (p->update_background_idle_cb)
    {
        g_source_remove(p->update_background_idle_cb);
        p->update_background_idle_cb = 0;
    }

    /* Redraw the top level widget. */
    if (gtk_widget_get_realized(p->topgwin))
    {
        panel_determine_background_pixmap(p, p->topgwin, p->topgwin->window);
        gdk_window_clear(p->topgwin->window);
    }
    gtk_widget_queue_draw(p->topgwin);
}

static gboolean delay_update_background( Panel* p )
{
    p->update_background_idle_cb = 0;

    /* Panel could be destroyed while background update scheduled */
    if ( p->topgwin && gtk_widget_get_realized( p->topgwin ) ) {
        gdk_display_sync( gtk_widget_get_display(p->topgwin) );
        panel_update_background( p );
    }

    return FALSE;
}

void panel_require_update_background( Panel* p )
{
    if (!p->update_background_idle_cb)
    {
        p->update_background_idle_cb = g_idle_add_full( G_PRIORITY_LOW,
            (GSourceFunc)delay_update_background, p, NULL );
    }
}

static void panel_realize(GtkWidget *widget, Panel *p)
{
    panel_require_update_background(p);
}

static void panel_style_set(GtkWidget *widget, GtkStyle* prev, Panel *p)
{
    /* FIXME: This dirty hack is used to fix the background of systray... */
    if( gtk_widget_get_realized( widget ) )
        panel_require_update_background(p);
}

/******************************************************************************/

/*= Panel size and position =*/

/* Calculate real width of a horizontal panel (or height of a vertical panel) */
static void calculate_width(int scrw, int wtype, int align, int margin, int *panw, int *x)
{
    su_log_debug2("panw=%d, margin=%d scrw=%d\n", *panw, margin, scrw);
    //scrw -= 2;
    if (wtype == WIDTH_PERCENT) {
        /* sanity check */
        if (*panw > 100)
            *panw = 100;
        else if (*panw < 0)
            *panw = 1;
        *panw = ((gfloat) scrw * (gfloat) *panw) / 100.0;
    }

    if (margin > scrw) {
        su_print_error_message("margin is bigger then edge size %d > %d. Ignoring margin\n", margin, scrw);
        margin = 0;
    }

    if (align == ALIGN_CENTER)
        margin = 0;

    *panw = MIN(scrw - margin, *panw);

    su_log_debug2("panw=%d\n", *panw);
    if (align == ALIGN_LEFT)
        *x += margin;
    else if (align == ALIGN_RIGHT) {
        *x += scrw - *panw - margin;
        if (*x < 0)
            *x = 0;
    } else if (align == ALIGN_CENTER)
        *x += (scrw - *panw) / 2;
}

/* Calculate panel size and position with given margins. */

static void calculate_position(Panel *np, int margin_top, int margin_bottom)
{
    int sswidth, ssheight, minx, miny;

    GdkScreen * screen = gtk_widget_get_screen(np->topgwin);

    /* FIXME: Why this doesn't work? */
    if (0)  {
//        if (np->curdesk < np->wa_len/4) {
        minx = np->workarea[np->curdesk*4 + 0];
        miny = np->workarea[np->curdesk*4 + 1];
        sswidth  = np->workarea[np->curdesk*4 + 2];
        ssheight = np->workarea[np->curdesk*4 + 3];
    } else {
        minx = miny = 0;
        sswidth  = gdk_screen_get_width(screen);
        ssheight = gdk_screen_get_height(screen);

        int monitor = -1;
        if (np->output_target == OUTPUT_PRIMARY_MONITOR)
            monitor = gdk_screen_get_primary_monitor(screen);
        else if (np->output_target == OUTPUT_CUSTOM_MONITOR)
            monitor = np->custom_monitor;

        if (monitor >= 0 && monitor < gdk_screen_get_n_monitors(screen))
        {
            GdkRectangle rect;
            rect.x = 0;
            rect.y = 0;
            rect.width = sswidth;
            rect.height = ssheight;
            gdk_screen_get_monitor_geometry(screen, monitor, &rect);
            sswidth = rect.width;
            ssheight = rect.height;
            minx = rect.x;
            miny = rect.y;
        }

        np->output_target_width = sswidth;
        np->output_target_height = ssheight;
    }

    int edge_margin = np->edge_margin;

    if (np->visibility_mode == VISIBILITY_AUTOHIDE || np->visibility_mode == VISIBILITY_GOBELOW)
    {
        edge_margin = 0;
    }

    switch (np->edge)
    {
        case EDGE_TOP   : miny     += edge_margin; break;
        case EDGE_BOTTOM: ssheight -= edge_margin; break;
        case EDGE_LEFT  : minx     += edge_margin; break;
        case EDGE_RIGHT : sswidth  -= edge_margin; break;
    }

    if (np->edge == EDGE_TOP || np->edge == EDGE_BOTTOM) {
        np->aw = np->oriented_width;
        np->ax = minx;
        calculate_width(sswidth, np->oriented_width_type, np->align, np->align_margin,
              &np->aw, &np->ax);
        np->ah = np->autohide_visible ? np->oriented_height : np->height_when_hidden;
        np->ay = miny + ((np->edge == EDGE_TOP) ? 0 : (ssheight - np->ah));

    } else {
        miny += margin_top;
        ssheight -= (margin_top + margin_bottom);

        np->ah = np->oriented_width;
        np->ay = miny;
        calculate_width(ssheight, np->oriented_width_type, np->align, np->align_margin,
              &np->ah, &np->ay);
        np->aw = np->autohide_visible ? np->oriented_height : np->height_when_hidden;
        np->ax = minx + ((np->edge == EDGE_LEFT) ? 0 : (sswidth - np->aw));
    }
    //g_debug("%s - x=%d y=%d w=%d h=%d\n", __FUNCTION__, np->ax, np->ay, np->aw, np->ah);
}

/* Calculate panel size and position. */

static void panel_calculate_position(Panel *p)
{
    int margin_top = 0;
    int margin_bottom = 0;

    if (p->edge == EDGE_LEFT || p->edge == EDGE_RIGHT)
    {
        GSList* l;
        for( l = all_panels; l; l = l->next )
        {
            Panel* lp = (Panel*)l->data;
            if (!lp->visible || lp->visibility_mode == VISIBILITY_AUTOHIDE || !lp->set_strut)
                continue;
            if (lp->edge == EDGE_TOP && lp->ch > margin_top)
                margin_top = lp->ch;
            else if (lp->edge == EDGE_BOTTOM && lp->ch > margin_bottom)
                margin_bottom = lp->ch;
        }
    }

    calculate_position(p, margin_top, margin_bottom);
}

/* Force panel geometry update. */

void update_panel_geometry(Panel* p)
{
    /* Guard against being called early in panel creation. */
    if (p->topgwin != NULL)
    {
        panel_calculate_position(p);
        if (p->oriented_width_type == WIDTH_REQUEST || p->oriented_height_type == HEIGHT_REQUEST)
        {
            gtk_widget_set_size_request(p->topgwin, -1, -1);
            gtk_widget_queue_resize(p->topgwin);
        }
        else
        {
            gtk_widget_set_size_request(p->topgwin, p->aw, p->ah);
        }
        gdk_window_move(p->topgwin->window, p->ax, p->ay);
        panel_update_background(p);
        panel_establish_autohide(p);
        panel_set_wm_state(p);
        panel_set_wm_strut(p);
        panel_set_desktop_icon_overlap_mode(p);
    }
}

/* size-request signal handler */

static gint panel_size_req(GtkWidget *widget, GtkRequisition *req, Panel *p)
{
    if (p->oriented_width_type == WIDTH_REQUEST)
        p->oriented_width = (p->orientation == ORIENT_HORIZ) ? req->width : req->height;
    if (p->oriented_height_type == HEIGHT_REQUEST)
        p->oriented_height = (p->orientation == ORIENT_HORIZ) ? req->height : req->width;
    panel_calculate_position(p);
    req->width  = p->aw;
    req->height = p->ah;

    return TRUE;
}

static void panel_size_position_changed(Panel *p, gboolean position_changed)
{
    if (position_changed)
    {
        if (p->bg)
            fb_bg_notify_changed_bg(p->bg);
    }

    panel_set_wm_strut(p);
    panel_set_desktop_icon_overlap_mode(p);

    if (position_changed)
    {
        if (p->edge == EDGE_TOP || p->edge == EDGE_BOTTOM)
        {
            GSList* l;
            for( l = all_panels; l; l = l->next )
            {
                Panel* lp = (Panel*)l->data;
                if (lp->edge == EDGE_LEFT || lp->edge == EDGE_RIGHT)
                {
                    update_panel_geometry(lp);
                }
            }
        }
    }

    if (p->stretch_background)
        panel_update_background(p);
}

/* size-allocate signal handler */

static gint panel_size_alloc(GtkWidget *widget, GtkAllocation *a, Panel *p)
{
    if (p->oriented_width_type == WIDTH_REQUEST)
        p->oriented_width = (p->orientation == ORIENT_HORIZ) ? a->width : a->height;
    if (p->oriented_height_type == HEIGHT_REQUEST)
        p->oriented_height = (p->orientation == ORIENT_HORIZ) ? a->height : a->width;

    //g_print("size-alloc: %d, %d, %d, %d\n", a->x, a->y, a->width, a->height);

    panel_calculate_position(p);
    gtk_window_move(GTK_WINDOW(p->topgwin), p->ax, p->ay);

    /* a->x and a->y always contain 0. */
    if (a->width == p->cw && a->height == p->ch) {
        return TRUE;
    }

    p->cw = a->width;
    p->ch = a->height;

    //g_print("req: %d, %d, %d, %d\n", p->ax, p->ay, p->aw, p->ah);

    panel_size_position_changed(p , FALSE);

    return TRUE;
}

/* configure-event signal handler */

static  gboolean panel_configure_event (GtkWidget *widget, GdkEventConfigure *e, Panel *p)
{
    gboolean position_changed = e->x != p->cx || e->y != p->cy;
    gboolean size_changed = e->width != p->cw || e->height != p->ch;

    if (!position_changed && !size_changed)
        return TRUE;

    p->cw = e->width;
    p->ch = e->height;
    p->cx = e->x;
    p->cy = e->y;

    //g_print("configure: %d, %d, %d, %d\n", p->cx, p->cy, p->cw, p->ch);

    panel_size_position_changed(p, position_changed);

    return FALSE;
}

static void panel_screen_monitors_changed_event(GdkScreen * _screen, Panel * panel)
{
    update_panel_geometry(panel);
}

static void panel_screen_size_changed_event(GdkScreen * _screen, Panel * panel)
{
    update_panel_geometry(panel);
}

/******************************************************************************/

static gboolean panel_can_be_drag_moved(Panel * panel)
{
    if (wtl_is_in_kiosk_mode())
        return FALSE;
    return (panel->visibility_mode == VISIBILITY_ALWAYS) || (panel->visibility_mode == VISIBILITY_BELOW);
}

gboolean panel_handle_drag_move(Panel * panel, GdkEventButton * event)
{
    if (event->type == GDK_BUTTON_PRESS)
    {
        if (event->button == 1 && event->state & GDK_CONTROL_MASK)
        {
            if (!panel->doing_panel_drag_move && panel_can_be_drag_moved(panel))
            {
                //GdkCursor * cursor = gdk_cursor_new_for_display(panel->display, GDK_FLEUR);
                GdkCursor * cursor = gdk_cursor_new_from_name(panel->display, "fleur");

                GdkGrabStatus grab_status = gdk_pointer_grab(
                    panel_get_toplevel_window(panel),
                    FALSE,
                    GDK_ALL_EVENTS_MASK,
                    NULL,
                    cursor,
                    event->time);

                if (grab_status == GDK_GRAB_SUCCESS)
                {
                    gdk_window_set_cursor(panel_get_toplevel_window(panel), cursor);

                    panel->doing_panel_drag_move = TRUE;
                    panel->panel_drag_move_start_x = event->x_root;
                    panel->panel_drag_move_start_y = event->y_root;
                    panel->panel_drag_move_start_edge_margin = panel->edge_margin;
                    panel->panel_drag_move_start_align_margin = panel->align_margin;
                }

                gdk_cursor_unref(cursor);
            }
            return TRUE;
        }
        return FALSE;
    }

    if (!panel->doing_panel_drag_move)
    {
        return FALSE;
    }

    int x_offset = event->x_root - panel->panel_drag_move_start_x;
    int y_offset = event->y_root - panel->panel_drag_move_start_y;

    int edge_margin_offset = 0;
    int align_margin_offset = 0;

    switch (panel->edge)
    {
        case EDGE_TOP:
        {
            edge_margin_offset = panel->panel_drag_move_start_edge_margin + y_offset;
            if (panel->align == ALIGN_LEFT)
                align_margin_offset = panel->panel_drag_move_start_align_margin + x_offset;
            else if (panel->align == ALIGN_RIGHT)
                align_margin_offset = panel->panel_drag_move_start_align_margin - x_offset;
            break;
        }
        case EDGE_LEFT:
        {
            edge_margin_offset = panel->panel_drag_move_start_edge_margin + x_offset;
            if (panel->align == ALIGN_LEFT)
                align_margin_offset = panel->panel_drag_move_start_align_margin + y_offset;
            else if (panel->align == ALIGN_RIGHT)
                align_margin_offset = panel->panel_drag_move_start_align_margin - y_offset;
            break;
        }
        case EDGE_BOTTOM:
        {
            edge_margin_offset = panel->panel_drag_move_start_edge_margin - y_offset;
            if (panel->align == ALIGN_LEFT)
                align_margin_offset = panel->panel_drag_move_start_align_margin + x_offset;
            else if (panel->align == ALIGN_RIGHT)
                align_margin_offset = panel->panel_drag_move_start_align_margin - x_offset;
            break;
        }
        case EDGE_RIGHT:
        {
            edge_margin_offset = panel->panel_drag_move_start_edge_margin - x_offset;
            if (panel->align == ALIGN_LEFT)
                align_margin_offset = panel->panel_drag_move_start_align_margin + y_offset;
            else if (panel->align == ALIGN_RIGHT)
                align_margin_offset = panel->panel_drag_move_start_align_margin - y_offset;
            break;
        }
    }

    if (edge_margin_offset < 0)
        edge_margin_offset = 0;
    if (align_margin_offset < 0)
        align_margin_offset = 0;

    panel->edge_margin = edge_margin_offset;
    panel->align_margin = align_margin_offset;

    update_panel_geometry(panel);

    if (!(event->state & GDK_BUTTON1_MASK)
    ||   (event->type == GDK_BUTTON_RELEASE && event->button == 1)
    ||  !(panel_can_be_drag_moved(panel)))
    {
        gdk_pointer_ungrab(event->time);
        gdk_window_set_cursor(panel_get_toplevel_window(panel), NULL);
        panel->doing_panel_drag_move = FALSE;
        panel_save_configuration(panel);
        return TRUE;
    }

    return TRUE;
}

/******************************************************************************/

void panel_button_press_hack(Panel *panel)
{
    /*
        XXX:
        Openbox raises a _NET_WM_WINDOW_TYPE_DOCK window on click even if it has _NET_WM_STATE_BELOW flag.
        We have to explicitly lower the panel on every click.
    */
    if (panel->visibility_mode == VISIBILITY_BELOW)
        panel_set_wm_state(panel);
}

/* Handler for "button_press_event" signal with Panel as parameter. */
static gboolean panel_button_press_event_with_panel(GtkWidget *widget, GdkEventButton *event, Panel *panel)
{
    panel_button_press_hack(panel);

    if (event->button == 3)	 /* right button */
    {
        panel_show_panel_menu(panel, NULL, event);
        return TRUE;
    }

    return panel_handle_drag_move(panel, event);
}

static gboolean panel_release_event(GtkWidget * widget, GdkEventButton * event, Panel * panel)
{
    return panel_handle_drag_move(panel, event);
}

static gboolean panel_motion_notify_event(GtkWidget * widget, GdkEventButton * event, Panel * panel)
{
    return panel_handle_drag_move(panel, event);
}

/******************************************************************************/

/* If there is a panel on this edge and it is not the panel being configured, set the edge unavailable. */
static gboolean panel_edge_available(Panel* p, int edge)
{
    GSList* l;
    for (l = get_all_panels(); l != NULL; l = l->next)
    {
        Panel* pl = (Panel*) l->data;
        if ((pl != p) && (pl->edge == edge))
            return FALSE;
    }
    return TRUE;
}

/******************************************************************************/

static char* gen_panel_name( int edge )
{
    const char * edge_str = su_enum_to_str(edge_pair, edge, "");
    gchar * name = NULL;
    gchar * dir = wtl_get_config_path("panels", SU_PATH_CONFIG_USER_W);
    int i;
    for( i = 1; i < G_MAXINT; ++i )
    {
        name =  g_strdup_printf( "%s%d", edge_str, i );

        gchar * file_name = g_strdup_printf("%s" PANEL_FILE_SUFFIX, name);

        gchar * path = g_build_filename( dir, file_name, NULL );

        g_free(file_name);

        if( ! g_file_test( path, G_FILE_TEST_EXISTS ) )
        {
            g_free( path );
            break;
        }
        g_free( name );
        g_free( path );
    }
    g_free( dir );
    return name;
}

static void try_allocate_edge(Panel* p, int edge)
{
    if ((p->edge == EDGE_NONE) && (panel_edge_available(p, edge)))
        p->edge = edge;
}

void create_empty_panel(void)
{
    Panel* new_panel = panel_allocate();

    /* Allocate the edge. */
    try_allocate_edge(new_panel, EDGE_BOTTOM);
    try_allocate_edge(new_panel, EDGE_TOP);
    try_allocate_edge(new_panel, EDGE_LEFT);
    try_allocate_edge(new_panel, EDGE_RIGHT);
    if (new_panel->edge == EDGE_NONE)
        new_panel->edge = EDGE_BOTTOM;
    new_panel->name = gen_panel_name(new_panel->edge);

    panel_configure(new_panel, 0);
    panel_normalize_configuration(new_panel);
    panel_start_gui(new_panel);
    gtk_widget_show_all(new_panel->topgwin);

    panel_save_configuration(new_panel);
    all_panels = g_slist_prepend(all_panels, new_panel);
}

/******************************************************************************/

void delete_panel(Panel * panel)
{
    all_panels = g_slist_remove( all_panels, panel );

    /* delete the config file of this panel */
    gchar * dir = wtl_get_config_path("panels", SU_PATH_CONFIG_USER_W);
    gchar * file_name = g_strdup_printf("%s" PANEL_FILE_SUFFIX, panel->name);
    gchar * file_path = g_build_filename( dir, file_name, NULL );

    g_unlink( file_path );

    g_free(file_path);
    g_free(file_name);
    g_free(dir);

    panel->config_changed = 0;
    panel_destroy(panel);
}

/******************************************************************************/

GSList * get_all_panels()
{
    return all_panels;
}

GList * panel_get_plugins(Panel * p)
{
    return p->plugins;
}

/******************************************************************************/

/* Set an image from a file with scaling to the panel icon size. */
void panel_image_set_from_file(Panel * p, GtkWidget * image, char * file)
{
    GdkPixbuf * pixbuf = gdk_pixbuf_new_from_file_at_scale(file, panel_get_icon_size(p), panel_get_icon_size(p), TRUE, NULL);
    if (pixbuf != NULL)
    {
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
        g_object_unref(pixbuf);
    }
}

/* Set an image from a icon theme with scaling to the panel icon size. */
gboolean panel_image_set_icon_theme(Panel * p, GtkWidget * image, const gchar * icon)
{
    if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), icon))
    {
        GdkPixbuf * pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), icon, panel_get_icon_size(p), 0, NULL);
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
        g_object_unref(pixbuf);
        return TRUE;
    }
    return FALSE;
}

/******************************************************************************/

/*= panel creation =*/

/* Force Pango initialization to avoid confusing timings in callgring log. */
static void
panel_force_pango_initialization(Panel * p)
{
    static int initialized = 0;
    if (initialized)
        return;

    PangoRectangle logical_rect;
    PangoLayout * layout = gtk_widget_create_pango_layout(p->topgwin, NULL);
    pango_layout_set_text (layout, "0", -1);
    pango_layout_get_pixel_extents(layout, NULL, &logical_rect);
    g_object_unref(layout);

    initialized = 1;
}

static gboolean panel_on_composite_check_timeout(Panel * p)
{
    gboolean is_composited = panel_is_composited(p);
    if (p->is_composited != is_composited)
    {
        p->is_composited = is_composited;
        panel_update_background(p);
        panel_notify_plugins_on_compositing_mode_change(p);
    }
    return TRUE;
}

static void
panel_start_gui(Panel *p)
{
    p->curdesk = get_net_current_desktop();
    p->desknum = get_net_number_of_desktops();
    p->workarea = wtl_x11_get_xa_property (GDK_ROOT_WINDOW(), a_NET_WORKAREA, XA_CARDINAL, &p->wa_len);

    /* main toplevel window */
    /* p->topgwin =  gtk_window_new(GTK_WINDOW_TOPLEVEL); */
    p->topgwin = (GtkWidget*)g_object_new(PANEL_TOPLEVEL_TYPE, NULL);
    gtk_widget_set_name(p->topgwin, p->widget_name);
    p->display = gdk_display_get_default();

    /* Set colormap. */
    p->screen = gtk_widget_get_screen(p->topgwin);
    GdkColormap * colormap = NULL;
    p->alpha_channel_support = FALSE;
    if (strcmp(force_colormap, "rgba") == 0)
    {
        colormap = gdk_screen_get_rgba_colormap(p->screen);
        if (colormap != NULL)
            p->alpha_channel_support = TRUE;
    }
    else if (strcmp(force_colormap, "rgb") == 0)
    {
        colormap = gdk_screen_get_rgb_colormap(p->screen);
    }
    else if (strcmp(force_colormap, "system") == 0)
    {
        colormap = gdk_screen_get_system_colormap(p->screen);
    }

    if (colormap)
        gtk_widget_set_colormap(p->topgwin, colormap);


    gtk_container_set_border_width(GTK_CONTAINER(p->topgwin), 0);
    gtk_window_set_resizable(GTK_WINDOW(p->topgwin), FALSE);
    gtk_window_set_wmclass(GTK_WINDOW(p->topgwin), "panel", "waterline");
    gtk_window_set_title(GTK_WINDOW(p->topgwin), p->name);
    gtk_window_set_position(GTK_WINDOW(p->topgwin), GTK_WIN_POS_NONE);
    gtk_window_set_decorated(GTK_WINDOW(p->topgwin), FALSE);

    gtk_window_group_add_window( window_group, (GtkWindow*)p->topgwin );

    g_signal_connect(G_OBJECT(p->screen), "monitors-changed",
          (GCallback) panel_screen_monitors_changed_event, p);
    g_signal_connect(G_OBJECT(p->screen), "size-changed",
          (GCallback) panel_screen_size_changed_event, p);

    g_signal_connect(G_OBJECT(p->topgwin), "delete-event",
          G_CALLBACK(panel_delete_event), p);
    g_signal_connect(G_OBJECT(p->topgwin), "destroy-event",
          G_CALLBACK(panel_destroy_event), p);
    g_signal_connect (G_OBJECT (p->topgwin), "size-request",
          (GCallback) panel_size_req, p);
    g_signal_connect (G_OBJECT (p->topgwin), "size-allocate",
          (GCallback) panel_size_alloc, p);
    g_signal_connect (G_OBJECT (p->topgwin), "configure-event",
          (GCallback) panel_configure_event, p);

    gtk_widget_add_events( p->topgwin, GDK_BUTTON_PRESS_MASK );
    g_signal_connect(G_OBJECT (p->topgwin), "button_press_event",
          (GCallback) panel_button_press_event_with_panel, p);

    g_signal_connect(G_OBJECT (p->topgwin), "button_release_event", G_CALLBACK(panel_release_event), (gpointer) p);
    g_signal_connect(G_OBJECT (p->topgwin), "motion-notify-event", G_CALLBACK(panel_motion_notify_event), (gpointer) p);


    g_signal_connect (G_OBJECT (p->topgwin), "realize",
          (GCallback) panel_realize, p);

    g_signal_connect (G_OBJECT (p->topgwin), "style-set",
          (GCallback)panel_style_set, p);
    gtk_widget_realize(p->topgwin);
    //gdk_window_set_decorations(p->topgwin->window, 0);

    // containers
    p->toplevel_alignment = gtk_alignment_new(0, 0, 0, 0);
    p->plugin_box = p->my_box_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(p->toplevel_alignment), 0);
    gtk_container_set_border_width(GTK_CONTAINER(p->plugin_box), 0);
    gtk_container_add(GTK_CONTAINER(p->topgwin), p->toplevel_alignment);
    gtk_container_add(GTK_CONTAINER(p->toplevel_alignment), p->plugin_box);
    gtk_widget_show(p->toplevel_alignment);
    gtk_widget_show(p->plugin_box);
    panel_update_toplevel_alignment(p);

    gtk_alignment_set_padding(GTK_ALIGNMENT(p->toplevel_alignment),
        p->padding_top, p->padding_bottom, p->padding_left, p->padding_right);
    gtk_box_set_spacing(GTK_BOX(p->plugin_box), p->applet_spacing);

    p->topxwin = GDK_WINDOW_XWINDOW(GTK_WIDGET(p->topgwin)->window);
    su_log_debug("topxwin = %x\n", p->topxwin);

    /* the settings that should be done before window is mapped */
    wm_noinput(p->topxwin);
    panel_set_dock_type(p);

    /* window mapping point */
    gtk_widget_show_all(p->topgwin);

    /*
        HACK:
        Есть баг: отрисовка панели динамического размера зависает после старта.
        Если панель передвинуть, отрисовка отвисает.
        Так что этот gtk_window_move() -- хак, чтобы развисла отрисовка.
        До истинных причин бага я не докопался.
    */
    gtk_window_move(GTK_WINDOW(p->topgwin), -1, -1);

    gdk_window_set_accept_focus(gtk_widget_get_window(p->topgwin), FALSE);

    /* the settings that should be done after window is mapped */
    panel_establish_autohide(p);

    /* send it to running wm */
    Xclimsg(p->topxwin, a_NET_WM_DESKTOP, 0xFFFFFFFF, 0, 0, 0, 0);
    /* and assign it ourself just for case when wm is not running */
    guint32 val = 0xFFFFFFFF;
    XChangeProperty(GDK_DISPLAY(), p->topxwin, a_NET_WM_DESKTOP, XA_CARDINAL, 32,
          PropModeReplace, (unsigned char *) &val, 1);

    panel_set_wm_state(p);

    panel_calculate_position(p);
    gdk_window_move_resize(p->topgwin->window, p->ax, p->ay, p->aw, p->ah);
    panel_set_wm_strut(p);

    panel_set_desktop_icon_overlap_mode(p);

    panel_force_pango_initialization(p);

    p->is_composited = panel_is_composited(p);
    if (panel_is_composite_available(p))
    {
        p->composite_check_timeout = g_timeout_add(3 * 1000, (GSourceFunc) panel_on_composite_check_timeout, p);
    }
}

/* Exchange the "width" and "height" terminology for vertical and horizontal panels. */
void panel_adjust_geometry_terminology(Panel * p)
{
    if ((p->pref_dialog.height_label != NULL) && (p->pref_dialog.width_label != NULL)
    && (p->pref_dialog.alignment_left_label != NULL) && (p->pref_dialog.alignment_right_label != NULL))
    {
        char * edge_align_text = "";
        switch (p->edge)
        {
            case EDGE_TOP   : edge_align_text = _("Top margin:"); break;
            case EDGE_BOTTOM: edge_align_text = _("Bottom margin:"); break;
            case EDGE_LEFT  : edge_align_text = _("Left margin:"); break;
            case EDGE_RIGHT : edge_align_text = _("Right margin:"); break;
        }
        gtk_label_set_text(GTK_LABEL(p->pref_dialog.edge_margin_label), edge_align_text);

        if ((p->edge == EDGE_TOP) || (p->edge == EDGE_BOTTOM))
        {
            gtk_label_set_text(GTK_LABEL(p->pref_dialog.height_label), _("Height:"));
            gtk_label_set_text(GTK_LABEL(p->pref_dialog.width_label), _("Width:"));
            gtk_button_set_label(GTK_BUTTON(p->pref_dialog.alignment_left_label), _("Left"));
            gtk_button_set_label(GTK_BUTTON(p->pref_dialog.alignment_right_label), _("Right"));
            if (p->align == ALIGN_RIGHT)
                gtk_label_set_text(GTK_LABEL(p->pref_dialog.align_margin_label), _("Right margin:"));
            else
                gtk_label_set_text(GTK_LABEL(p->pref_dialog.align_margin_label), _("Left margin:"));
        }
        else
        {
            gtk_label_set_text(GTK_LABEL(p->pref_dialog.height_label), _("Width:"));
            gtk_label_set_text(GTK_LABEL(p->pref_dialog.width_label), _("Height:"));
            gtk_button_set_label(GTK_BUTTON(p->pref_dialog.alignment_left_label), _("Top"));
            gtk_button_set_label(GTK_BUTTON(p->pref_dialog.alignment_right_label), _("Bottom"));
            if (p->align == ALIGN_RIGHT)
                gtk_label_set_text(GTK_LABEL(p->pref_dialog.align_margin_label), _("Bottom margin:"));
            else
                gtk_label_set_text(GTK_LABEL(p->pref_dialog.align_margin_label), _("Top margin:"));
        }
    }
}

void panel_draw_label_text(Panel * p, GtkWidget * label, const char * text, unsigned style)
{
    panel_draw_label_text_with_font(p, label, text, style, NULL);
}

void panel_draw_label_text_with_font(Panel * p, GtkWidget * label, const char * text, unsigned style, const char * custom_font_desc)
{
    gboolean bold = style & STYLE_BOLD;
    gboolean italic  = style & STYLE_ITALIC;
    gboolean underline = style & STYLE_UNDERLINE;
    gboolean custom_color = style & STYLE_CUSTOM_COLOR;
    gboolean allow_markup = style & STYLE_MARKUP;

    if (text == NULL)
    {
        gtk_label_set_text(GTK_LABEL(label), NULL);
        return;
    }

    /* Compute an appropriate size so the font will scale with the panel's icon size. */
    int font_size = 0;

    if (p->use_font_size)
    {
        font_size = p->font_size;
        if (p->font_size == 0)
        {
            if (panel_get_icon_size(p) < 20)
               font_size = 9;
            else if (panel_get_icon_size(p) >= 20 && panel_get_icon_size(p) < 36)
               font_size = 10;
            else
               font_size = 12;
        }
    }

    /* Check the string for characters that need to be escaped.
     * If any are found, create the properly escaped string and use it instead. */
    const char * valid_markup = text;
    char * escaped_text = NULL;
    if (!allow_markup)
    {
        const char * q;
        for (q = text; *q != '\0'; q += 1)
        {
            if ((*q == '<') || (*q == '>') || (*q == '&'))
            {
                escaped_text = g_markup_escape_text(text, -1);
                valid_markup = escaped_text;
                break;
            }
        }
    }

    gchar * attr_color = "";
    gchar * attr_color_allocated = NULL;
    if ((custom_color) && (p->use_font_color))
        attr_color_allocated =  attr_color = g_strdup_printf(" color=\"#%06x\"", gcolor2rgb24(&p->font_color));

    gchar * attr_desc = "";
    gchar * attr_desc_allocated = NULL;
    if (!su_str_empty(custom_font_desc))
    {
        gchar * custom_font_desc_escaped = g_markup_escape_text(custom_font_desc, -1);
        attr_desc_allocated = attr_desc = g_strdup_printf(" font_desc=\"%s\"", custom_font_desc_escaped);
        g_free(custom_font_desc_escaped);
    }
    else if (font_size > 0)
    {
        attr_desc_allocated = attr_desc = g_strdup_printf(" font_desc=\"%d\"", font_size);
    }

    gchar * markup_text = g_strdup_printf("<span%s%s>%s%s%s%s%s%s%s</span>",
            attr_desc, attr_color,
            ((bold) ? "<b>" : ""),
            ((italic) ? "<i>" : ""),
            ((underline) ? "<u>" : ""),
            valid_markup,
            ((underline) ? "</u>" : ""),
            ((italic) ? "</i>" : ""),
            ((bold) ? "</b>" : ""));
    gtk_label_set_markup(GTK_LABEL(label), markup_text);

    g_free(markup_text);

    g_free(attr_desc_allocated);
    g_free(attr_color_allocated);
    g_free(escaped_text);

}

void panel_update_toplevel_alignment(Panel *p)
{
    gboolean expand = FALSE;
    GList * l;
    for (l = p->plugins; l; l = l->next)
    {
        Plugin * pl = (Plugin *) l->data;
        if (pl->expand)
        {
            expand = TRUE;
            break;
        }
    }

    float scale = expand ? 1 : 0;

    float align = 0.5;
    if (p->align == ALIGN_LEFT)
        align = 0;
    else if (p->align == ALIGN_RIGHT)
        align = 1;

    if (p->orientation == ORIENT_HORIZ)
        gtk_alignment_set(GTK_ALIGNMENT(p->toplevel_alignment), align, 0.5, scale, 1);
    else
        gtk_alignment_set(GTK_ALIGNMENT(p->toplevel_alignment), 0.5, align, 1, scale);
}

static void panel_notify_plugins_on_configuration_change(Panel *p)
{
    GList* l;
    for (l = p->plugins; l; l = l->next) {
        Plugin* pl = (Plugin*)l->data;
        if (pl->class->panel_configuration_changed) {
            pl->class->panel_configuration_changed(pl);
        }
    }
}

static void panel_notify_plugins_on_compositing_mode_change(Panel *p)
{
    GList* l;
    for (l = p->plugins; l; l = l->next) {
        Plugin* pl = (Plugin*)l->data;
        if (pl->class->compositing_mode_changed) {
            pl->class->compositing_mode_changed(pl);
        } else if (pl->class->panel_configuration_changed) {
            pl->class->panel_configuration_changed(pl);
        }
    }
}

void panel_set_panel_configuration_changed(Panel *p)
{
    panel_set_desktop_icon_overlap_mode(p);

    if (p->toplevel_alignment)
        gtk_alignment_set_padding(GTK_ALIGNMENT(p->toplevel_alignment),
            p->padding_top, p->padding_bottom, p->padding_left, p->padding_right);
    if (p->plugin_box)
        gtk_box_set_spacing(GTK_BOX(p->plugin_box), p->applet_spacing);

    int previous_orientation = p->orientation;
    p->orientation = (p->edge == EDGE_TOP || p->edge == EDGE_BOTTOM)
        ? ORIENT_HORIZ : ORIENT_VERT;

    if (previous_orientation != p->orientation)
    {
        panel_adjust_geometry_terminology(p);
        if (p->pref_dialog.height_control != NULL)
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->pref_dialog.height_control), p->oriented_height);
        if ((p->oriented_width_type == WIDTH_PIXEL) && (p->pref_dialog.width_control != NULL))
        {
            int value = ((p->orientation == ORIENT_HORIZ) ? gdk_screen_width() : gdk_screen_height());
            gtk_spin_button_set_range(GTK_SPIN_BUTTON(p->pref_dialog.width_control), 0, value);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->pref_dialog.width_control), value);
        }

    }

    if (p->orientation == ORIENT_HORIZ) {
        p->my_box_new = gtk_hbox_new;
    } else {
        p->my_box_new = gtk_vbox_new;
    }

    /* recreate the main layout box */
    if (p->plugin_box != NULL)
    {
        GtkOrientation bo = (p->orientation == ORIENT_HORIZ) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
        gtk_orientable_set_orientation(GTK_ORIENTABLE(p->plugin_box), bo);
        panel_update_toplevel_alignment(p);
    }

    panel_notify_plugins_on_configuration_change(p);
}

static int
panel_parse_plugin(Panel *p, json_t * json_plugin)
{
    Plugin * plugin = NULL;
    gchar * type = NULL;

    su_json_dot_get_string(json_plugin, "type", "", &type);

    if (su_str_empty(type) || !(plugin = plugin_load(type))) {
        su_print_error_message( "can't load %s plugin\n", type ? type : "(null)");
        goto error;
    }

    plugin->panel = p;
    if (plugin->class->expand_available)
        plugin->expand = su_json_dot_get_bool(json_plugin, "expand", FALSE);
    plugin->padding = su_json_dot_get_int(json_plugin, "padding", 0);
    plugin->border = su_json_dot_get_int(json_plugin, "border", 0);

    json_decref(plugin->json);
    plugin->json = json_incref(json_plugin);

    if (!plugin_start(plugin)) {
        su_print_error_message( "can't start plugin %s\n", type);
        goto error;
    }

    p->plugins = g_list_append(p->plugins, plugin);

    g_free(type);

    return 1;

 error:
    if (plugin != NULL)
        plugin_unload(plugin);
    g_free(type);

    return 0;
}

static gchar * format_json_error(const json_error_t * error, const char * source)
{
    gchar * result = g_strdup_printf("%s:%d:%d: %s",
        source ? source : error->source,
        error->line,
        error->column,
        error->text);
    return result;
}

int panel_start(Panel *p, const char * configuration, const char * source)
{
    json_error_t error;
    json_t * json = json_loads(configuration, 0, &error);
    if (!json)
    {
        gchar * error_message = format_json_error(&error, source);
        su_print_error_message("%s\n", error_message);
        g_free(error_message);
        return 0;
    }

    if (!json_is_object(json))
    {
        su_print_error_message("%s: toplevel item should be object\n", source);
        json_decref(json);
        return 0;
    }

    json_decref(p->json);
    p->json = json;

    json_t * json_global = json_object_get(json, "global");
    if (!json_is_object(json_global))
    {
        su_print_error_message( "%s: configuration file must contain global section\n", source);
    }

    panel_read_global_configuration_from_json_object(p);

    panel_normalize_configuration(p);

    panel_start_gui(p);

    json_t * json_plugins = json_object_get(json, "plugins");

    size_t index;
    json_t * json_plugin;
    json_array_foreach(json_plugins, index, json_plugin) {
        panel_parse_plugin(p, json_plugin);
    }

    panel_update_toplevel_alignment(p);
    panel_update_background(p);

    return 1;
}

static void
delete_plugin(gpointer data, gpointer udata)
{
    plugin_delete((Plugin *)data);
}

static void panel_destroy(Panel *p)
{
    json_decref(p->json);

    g_signal_handlers_disconnect_by_func(G_OBJECT(p->screen), panel_screen_monitors_changed_event, p);
    g_signal_handlers_disconnect_by_func(G_OBJECT(p->screen), panel_screen_size_changed_event, p);

    if (p->set_wm_strut_idle)
        g_source_remove(p->set_wm_strut_idle);

    if (p->composite_check_timeout)
        g_source_remove(p->composite_check_timeout);

    if (p->hide_timeout)
        g_source_remove(p->hide_timeout);

    if (p->update_background_idle_cb)
        g_source_remove(p->update_background_idle_cb);

    if (p->pref_dialog.pref_dialog != NULL)
        gtk_widget_destroy(p->pref_dialog.pref_dialog);
    if (p->pref_dialog.plugin_pref_dialog != NULL)
    {
        gtk_widget_destroy(p->pref_dialog.plugin_pref_dialog);
        p->pref_dialog.plugin_pref_dialog = NULL;
    }

    if (p->bg != NULL)
    {
        g_signal_handlers_disconnect_by_func(G_OBJECT(p->bg), on_root_bg_changed, p);
        g_object_unref(p->bg);
    }

    if (p->background_pixmap)
    {
        g_object_unref(G_OBJECT(p->background_pixmap));
    }

    if (p->config_changed)
        panel_save_configuration(p);

    g_list_foreach(p->plugins, delete_plugin, NULL);
    g_list_free(p->plugins);
    p->plugins = NULL;

    gtk_window_group_remove_window( window_group, GTK_WINDOW(  p->topgwin ) );

    if( p->topgwin )
        gtk_widget_destroy(p->topgwin);
    g_free(p->workarea);
    g_free( p->background_file );

    g_free( p->widget_name );

    gdk_flush();
    XFlush(GDK_DISPLAY());
    XSync(GDK_DISPLAY(), True);

    g_free( p->name );
    g_free(p);
}

static Panel * panel_new(const char * config_file, const char * config_name)
{
    gchar * fp;

    if (!config_file)
        return NULL;

    g_file_get_contents(config_file, &fp, NULL, NULL);
    if (!fp)
        return NULL;

    Panel* panel = panel_allocate();
    panel->orientation = ORIENT_NONE;
    panel->name = g_strdup(config_name);

    panel->widget_name = g_strdup("PanelToplevel");

    if (!panel_start(panel, fp, config_file))
    {
        su_print_error_message( "can't start panel\n");
        panel_destroy(panel);
        panel = NULL;
    }

    g_free(fp);

    return panel;
}

static gboolean start_all_panels(void)
{
    gchar * panel_dir = wtl_get_config_path("panels", SU_PATH_CONFIG_USER);
    gchar * panel_dir_w = wtl_get_config_path("panels", SU_PATH_CONFIG_USER_W);

    gboolean save_panels = (!wtl_is_in_kiosk_mode()) && (g_strcmp0(panel_dir, panel_dir_w) != 0);

    GDir* dir = g_dir_open(panel_dir, 0, NULL);

    if (!dir)
    {
        return all_panels != NULL;
    }

    const gchar* file_name;
    while ((file_name = g_dir_read_name(dir)) != NULL)
    {
        if (strchr(file_name, '~') == NULL && /* Skip editor backup files in case user has hand edited in this directory. */
            file_name[0] != '.' && /* Skip hidden files. */
            g_str_has_suffix(file_name, PANEL_FILE_SUFFIX))
        {
            char* name = g_strdup(file_name);
            name[strlen(name) - strlen(PANEL_FILE_SUFFIX)] = 0;

            char* panel_config = g_build_filename( panel_dir, file_name, NULL );

            su_log_debug("loading panel %s from %s\n", name, panel_config);

            Panel* panel = panel_new( panel_config, name );
            if (panel)
            {
                all_panels = g_slist_prepend(all_panels, panel);
                if (save_panels)
                    panel_save_configuration(panel);
            }

            g_free( panel_config );
            g_free( name );
        }
    }

    g_dir_close(dir);
    g_free(panel_dir);
    g_free(panel_dir_w);

    return all_panels != NULL;
}

/******************************************************************************/

int panel_handle_x_error(Display * d, XErrorEvent * ev)
{
    char buf[256];

    if (su_log_level >= SU_LOG_WARNING)
    {
        XGetErrorText(GDK_DISPLAY(), ev->error_code, buf, 256);
        su_log_warning("X error: %s\n", buf);
    }
    return 0;	/* Ignored */
}

int panel_handle_x_error_swallow_BadWindow_BadDrawable(Display * d, XErrorEvent * ev)
{
    if ((ev->error_code != BadWindow) && (ev->error_code != BadDrawable))
        panel_handle_x_error(d, ev);
    return 0;	/* Ignored */
}

/******************************************************************************/

/*= Lightweight lock related functions - X clipboard hacks =*/

#define CLIPBOARD_NAME "WATERLINE_SELECTION"

/*
 * clipboard_get_func - dummy get_func for gtk_clipboard_set_with_data ()
 */
static void clipboard_get_func(
    GtkClipboard *clipboard G_GNUC_UNUSED,
    GtkSelectionData *selection_data G_GNUC_UNUSED,
    guint info G_GNUC_UNUSED,
    gpointer user_data_or_owner G_GNUC_UNUSED)
{
}

/*
 * clipboard_clear_func - dummy clear_func for gtk_clipboard_set_with_data ()
 */
static void clipboard_clear_func(
    GtkClipboard *clipboard G_GNUC_UNUSED,
    gpointer user_data_or_owner G_GNUC_UNUSED)
{
}

/*
 * Lightweight version for checking single instance.
 * Try and get the CLIPBOARD_NAME clipboard instead of using file manipulation.
 *
 * Returns TRUE if successfully retrieved and FALSE otherwise.
 */
static gboolean check_main_lock(void)
{
    static const GtkTargetEntry targets[] = { { CLIPBOARD_NAME, 0, 0 } };
    gboolean retval = FALSE;
    GtkClipboard *clipboard;
    Atom atom;

    atom = gdk_x11_get_xatom_by_name(CLIPBOARD_NAME);

    XGrabServer(GDK_DISPLAY());

    if (XGetSelectionOwner(GDK_DISPLAY(), atom) != None)
        goto out;

    clipboard = gtk_clipboard_get(gdk_atom_intern(CLIPBOARD_NAME, FALSE));

    if (gtk_clipboard_set_with_data(clipboard, targets,
                                    G_N_ELEMENTS (targets),
                                    clipboard_get_func,
                                    clipboard_clear_func, NULL))
        retval = TRUE;

out:
    XUngrabServer (GDK_DISPLAY ());
    gdk_flush ();

    return retval;
}
#undef CLIPBOARD_NAME

/******************************************************************************/

static void print_stderr(const char *string, ...)
{
    va_list ap;
    va_start(ap, string);
    vfprintf(stderr, string, ap);
    va_end(ap);
}

static void usage(gboolean error)
{
    void (*print)(const char *string, ...) = error ? print_stderr : g_print;

    if (!error)
        print(_("waterline %s - A lightweight framework for desktop widgets and applets"), version);
    print("\n\n");
    print(_("Syntax: %s [options]"), "waterline");
    print("\n\n");
    print(_("Options:"));
    print("\n");
    print("  --help             %s\n", _("Print this help and exit"));
    print("  --version          %s\n", _("Print version and exit"));
    print("  --log <number>     %s\n", _("Set log level 0-5. 0 - none 5 - chatty"));
    print("  --profile <name>   %s\n", _("Use specified profile"));
    print("  --kiosk-mode       %s\n", _("Enable kiosk mode"));
    print("\n");
    print(_("Debug options:"));
    print("\n");
    print("  --quit-in-menu     %s\n", _("Display 'quit' command in popup menu"));
    print("  --colormap <name>  %s\n", _("Force specified colormap (rgba, rgb, system, default)"));
    print("  --force-compositing-wm-disabled\n"
            "                     %s\n", _("Behave as if no compositing wm avaiable"));
    print("  --force-composite-disabled\n"
            "                     %s\n", _("Behave as if there is no compositing support at all"));
    print("\n");
    print(_("Short options:"));
    print("\n");
    print("  -h                 %s\n", _("same as --help"));
    print("  -p                 %s\n", _("same as --profile"));
    print("  -v                 %s\n", _("same as --version"));
    print("\n");
    if (!error)
    {
        print(_("Report bugs to: <%s> or <%s>\nProgram home page: <%s>"),
            wtl_bugreporting,
            wtl_email,
            wtl_website);
    }
    print("\n\n");
}

/******************************************************************************/

int main(int argc, char *argv[], char *env[])
{
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--glib-mem-profiler") || !strcmp(argv[i], "--glib_mem_profiler")) {
           g_mem_set_vtable(glib_mem_profiler_table);
           break;
        }
    }

    _wtl_agent_id = su_path_resolve_agent_id_by_pointer(main, "waterline");
    su_path_register_default_agent_prefix(_wtl_agent_id, PACKAGE_INSTALLATION_PREFIX);

    setlocale(LC_CTYPE, "");

#if !GLIB_CHECK_VERSION(2,32,0)
    g_thread_init(NULL);
#endif
    gdk_threads_init();
    gdk_threads_enter();

    gtk_init(&argc, &argv);

#ifdef ENABLE_NLS
    gchar * locale_dir = su_path_resolve_resource(_wtl_agent_id, "locale", NULL);
    if (locale_dir)
    {
        bindtextdomain(GETTEXT_PACKAGE, locale_dir);
        g_free(locale_dir);
    }
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
#endif

    XSetLocaleModifiers("");
    XSetErrorHandler((XErrorHandler) panel_handle_x_error);

    su_x11_resolve_well_known_atoms(gdk_x11_get_default_xdisplay());
    update_net_supported();

#define NEXT_ARGUMENT(s) if (++i >= argc) { su_print_error_message(s); goto print_usage_and_exit; }

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(FALSE);
            exit(0);
        } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
            printf("%s %s\n\n", g_get_prgname(), version);
            printf(
                "Copyright (C) 2011-2013 Vadim Ushakov (<igeekless@gmail.com>)\n"
                "Copyright (C) 2006 Hong Jen Yee (PCMan)\n"
                "Copyright (C) 2006 Jim Huang (aka jserv)\n"
                "Copyright (C) 2002 Anatoly Asviyan (aka Arsen)\n"
                "Copyright (C) 2000 Peter Zelezny\n");
            printf("\n%s", wtl_license);
            exit(0);
        } else if (!strcmp(argv[i], "--log")) {
            NEXT_ARGUMENT("missing log level\n")
            su_log_level = atoi(argv[i]);
        } else if (!strcmp(argv[i], "--profile") || !strcmp(argv[i], "-p")) {
            NEXT_ARGUMENT("missing profile name\n")
            cprofile = g_strdup(argv[i]);
        } else if (!strcmp(argv[i], "--kiosk-mode")) {
            wtl_enable_kiosk_mode();
        } else if (!strcmp(argv[i], "--quit-in-menu")) {
            quit_in_menu = TRUE;
        } else if (!strcmp(argv[i], "--force-compositing-wm-disabled")) {
            force_compositing_wm_disabled = TRUE;
        } else if (!strcmp(argv[i], "--force-composite-disabled")) {
            force_composite_disabled = TRUE;
        } else if (!strcmp(argv[i], "--glib-mem-profiler") || !strcmp(argv[i], "--glib_mem_profiler")) {
            /* nothing */
        } else if (!strcmp(argv[i], "--colormap")) {
            NEXT_ARGUMENT("missing colormap argument\n");
            force_colormap = g_strdup(argv[i]);
        } else {
            su_print_error_message("unrecognized option: %s\n", argv[i]);
            goto print_usage_and_exit;
        }
    }

#undef NEXT_ARGUMENT

    /* Check for duplicated instances */
    if (!check_main_lock()) {
        printf("There is already an instance of waterline.  Now to exit\n");
        exit(2);
    }

    /* Add our own icons to the search path of icon theme */
    gchar * images_path = wtl_resolve_own_resource("", "images", 0);
    gtk_icon_theme_append_search_path(gtk_icon_theme_get_default(), images_path);
    g_free(images_path);

    fbev = fb_ev_new();
    window_group = gtk_window_group_new();

restart:
    is_restarting = FALSE;

    wtl_load_global_config();

	/* NOTE: StructureNotifyMask is required by XRandR
	 * See init_randr_support() in gdkscreen-x11.c of gtk+ for detail.
	 */
    XSelectInput (GDK_DISPLAY(), GDK_ROOT_WINDOW(), StructureNotifyMask|SubstructureNotifyMask|PropertyChangeMask);
    gdk_window_add_filter(gdk_get_default_root_window (), (GdkFilterFunc)panel_event_filter, NULL);

    if (!start_all_panels())
    {
        su_print_error_message("No panels started. Creating an empty panel...\n");
        create_empty_panel();
    }

    gtk_main();

    XSelectInput (GDK_DISPLAY(), GDK_ROOT_WINDOW(), NoEventMask);
    gdk_window_remove_filter(gdk_get_default_root_window (), (GdkFilterFunc)panel_event_filter, NULL);

    /* destroy all panels */
    g_slist_foreach( all_panels, (GFunc) panel_destroy, NULL );
    g_slist_free( all_panels );
    all_panels = NULL;

    wtl_free_global_config();

    if( is_restarting )
        goto restart;

    g_object_unref(window_group);
    g_object_unref(fbev);

    gdk_threads_leave();

    return 0;

print_usage_and_exit:
    usage(TRUE);
    return 1;
}
