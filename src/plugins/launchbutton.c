/**
 * Copyright (c) 2011-2012 Vadim Ushakov
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
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pty.h>

//#include <menu-cache.h>

//#include <sys/types.h>
//#include <sys/stat.h>
//#include <unistd.h>
//#include <fcntl.h>

#include "panel.h"
#include "misc.h"
//#include "plugin.h"
//#include "bg.h"
//#include "menu-policy.h"

#include "dbg.h"

struct _lb_t;

typedef struct {
    guint input_source_id;
    guint input_hup_source_id;
    guint input_err_source_id;
    guint process_source_id;
    GIOChannel * input_channel;
    pid_t child_pid;

    gboolean eof;

    gchar * input_buffer;

    gchar * command;

    struct _lb_t * lb;
} input_t;

typedef struct _lb_t {
    char * icon_path;
    char * title;
    char * tooltip;
    char * command1;
    char * command2;
    char * command3;

    char * command1_override;
    char * command2_override;
    char * command3_override;

    GtkWidget * button;
    GtkWidget * img;
    GtkWidget * label;

    Plugin * plug;

    input_t input_title;
    input_t input_icon;
    input_t input_tooltip;
    input_t input_general;

    int input_restart_interval;

    gboolean use_pipes;

    guint input_timeout;
} lb_t;

/*****************************************************************************/

static void lb_input(lb_t * lb, input_t * input, gchar * line);

/*****************************************************************************/

static int _set_nonblocking(int fd)
{
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}     

static gboolean input_on_child_input(GIOChannel *source, GIOCondition condition, input_t * input)
{
    int i;

    if (!source)
        return TRUE;

    gchar buf[1024];
    buf[0] = 0;
    gsize bytes_read = 0;

    input->eof |= (condition == G_IO_HUP) || (condition == G_IO_ERR);

    GIOStatus status = 0;
    
    if (!input->eof)
    {
        status = g_io_channel_read_chars(source, buf, 1023, &bytes_read, NULL);
        buf[bytes_read] = 0;
    }

    //g_print("condition %d\n", (int)condition);
    //g_print("bs %d\n", (int)bytes_read);
    //g_print("input->eof %d\n", (int)input->eof);

    input->eof |= (status == G_IO_STATUS_EOF) || (status == G_IO_STATUS_ERROR);

    if (bytes_read > 0)
    {
	gchar * newbuffer = g_strconcat(input->input_buffer ? input->input_buffer : "", buf, NULL);
	g_free(input->input_buffer);
	input->input_buffer = newbuffer;

	gchar ** lines = g_strsplit(input->input_buffer, "\n", 0);
	int line_nr = g_strv_length(lines);

	g_free(input->input_buffer);

	for (i = 0; i < line_nr - (input->eof ? 0 : 1); i++)
	{
	    int l = strlen(lines[i]);
	    if (lines[i][l - 1] == '\r')
		lines[i][l - 1] = 0;

	    //g_print("== %s\n", lines[i]);

	    lb_input(input->lb, input, lines[i]);
	}

	if (!input->eof)
	    input->input_buffer = g_strdup(lines[line_nr - 1]);

	g_strfreev(lines);
    }

    if (input->eof)
    {
	if (input->input_source_id)
	{
	    g_source_remove(input->input_source_id);
	    input->input_source_id = 0;
	}

	if (input->input_hup_source_id)
	{
	    g_source_remove(input->input_hup_source_id);
	    input->input_hup_source_id = 0;
	}

	if (input->input_err_source_id)
	{
	    g_source_remove(input->input_err_source_id);
	    input->input_err_source_id = 0;
	}

	if (input->input_channel)
	{
	    g_io_channel_shutdown(input->input_channel, FALSE, NULL);
	    g_io_channel_unref(input->input_channel);
	    input->input_channel = NULL;
	}
    }

    return input->eof ? FALSE : TRUE;
}

static void input_on_child_exit(GPid pid, gint status, input_t * input)
{
    input->child_pid = 0;
    input_on_child_input(input->input_channel, 0, input);
}

static input_stop(input_t * input)
{
    if (input->input_source_id)
    {
        g_source_remove(input->input_source_id);
        input->input_source_id = 0;
    }

    if (input->input_hup_source_id)
    {
        g_source_remove(input->input_hup_source_id);
        input->input_hup_source_id = 0;
    }

    if (input->input_err_source_id)
    {
        g_source_remove(input->input_err_source_id);
        input->input_err_source_id = 0;
    }

    if (input->input_channel)
    {
        g_io_channel_shutdown(input->input_channel, FALSE, NULL);
        g_io_channel_unref(input->input_channel);
        input->input_channel = NULL;
    }

    if (input->process_source_id)
    {
        g_source_remove(input->process_source_id);
        input->process_source_id = 0;
    }

    if (input->child_pid)
    {
        kill(input->child_pid, SIGKILL);
        int stat_loc;
        waitpid(input->child_pid, &stat_loc, 0);
        input->child_pid = 0;
    }

    if (input->input_buffer)
    {
        g_free(input->input_buffer);
        input->input_buffer = NULL;
    }

}

static input_start(input_t * input)
{
    input_stop(input);

    input->eof = TRUE;

    if (strempty(input->command))
        return;

    gboolean use_pty = TRUE;

    int fds[2];
    if (use_pty)
    {
        openpty(&fds[0], &fds[1], NULL, NULL, NULL);
    }
    else
    {
        pipe(fds);
    }

    pid_t pid = fork();

    if (pid == 0)
    {
        if (use_pty)
        {
            setsid();
            ioctl(fds[1], TIOCSCTTY, (char *)NULL);
        }

        close(fds[0]);

        dup2(fds[1],1);
        close(fds[1]);

        execlp ("sh", "sh", "-c", input->command, (char *) NULL);
        _exit(-1);
    }

    close(fds[1]);

    if (pid < 0)
    {
        close(fds[0]);
    }
    else
    {
        input->eof = FALSE;
        input->child_pid = pid;
        _set_nonblocking(fds[0]);
        input->input_channel = g_io_channel_unix_new(fds[0]);
        input->input_source_id = g_io_add_watch(input->input_channel, G_IO_IN, input_on_child_input, input);
        input->input_hup_source_id = g_io_add_watch(input->input_channel, G_IO_HUP, input_on_child_input, input);
        input->input_err_source_id = g_io_add_watch(input->input_channel, G_IO_ERR, input_on_child_input, input);
        input->process_source_id = g_child_watch_add(pid, input_on_child_exit, input);
    }

}

/*****************************************************************************/

/* Handler for "button-press-event" event from launch button. */
static void lb_input(lb_t * lb, input_t * input, gchar * line)
{
    if (input == &lb->input_title)
    {
        fb_button_set_label(lb->button, lb->plug->panel, line);
    }
    else if (input == &lb->input_tooltip)
    {
        gtk_widget_set_tooltip_text(lb->button, line);
    }
    else if (input == &lb->input_icon)
    {
        int icon_size = lb->plug->panel->icon_size;
        fb_button_set_from_file(lb->button, line, icon_size, icon_size, TRUE);
    }
    else if (input == &lb->input_general)
    {
        gchar ** parts = g_strsplit_set(line, " \t", 2);
        if (g_strv_length(parts) == 2)
        {
            if (g_ascii_strcasecmp(parts[0], "Title") == 0)
                fb_button_set_label(lb->button, lb->plug->panel, parts[1]);
            else if (g_ascii_strcasecmp(parts[0], "Tooltip") == 0)
                gtk_widget_set_tooltip_text(lb->button, parts[1]);
            else if (g_ascii_strcasecmp(parts[0], "IconPath") == 0 || g_ascii_strcasecmp(parts[0], "Icon") == 0)
	    {
		int icon_size = lb->plug->panel->icon_size;
		fb_button_set_from_file(lb->button, parts[1], icon_size, icon_size, TRUE);
	    }
            else if (g_ascii_strcasecmp(parts[0], "Command1") == 0)
            {
                 g_free(lb->command1_override);
                 lb->command1_override = g_strdup(parts[1]);
            }
            else if (g_ascii_strcasecmp(parts[0], "Command2") == 0)
            {
                 g_free(lb->command2_override);
                 lb->command2_override = g_strdup(parts[1]);
            }
            else if (g_ascii_strcasecmp(parts[0], "Command3") == 0)
            {
                 g_free(lb->command3_override);
                 lb->command3_override = g_strdup(parts[1]);
            }
        }
        g_strfreev(parts);
    }

}


/*****************************************************************************/

/* Handler for "button-press-event" event from launch button. */
static gboolean lb_press_event(GtkWidget * widget, GdkEventButton * event, lb_t * lb)
{
    /* Standard right-click handling. */
    if (event->state & GDK_CONTROL_MASK)
    {
        if (plugin_button_press_event(widget, event, lb->plug))
            return TRUE;
    }

    const char* command = NULL;

    if (event->button == 1)
       command = strempty(lb->command1_override) ? lb->command1 : lb->command1_override;
    else if (event->button == 2)
       command = strempty(lb->command2_override) ? lb->command2 : lb->command2_override;
    else if (event->button == 3)
       command = strempty(lb->command3_override) ? lb->command3 : lb->command3_override;

    if (!strempty(command))
    {
        lxpanel_launch(command, NULL);
    }
    else
    {
        lxpanel_show_panel_menu( lb->plug->panel, lb->plug, event );
    }

    return TRUE;
}


static gboolean lb_input_timeout(lb_t * lb)
{
    if (lb->input_title.eof)
        input_start(&lb->input_title);
    if (lb->input_tooltip.eof)
        input_start(&lb->input_tooltip);
    if (lb->input_icon.eof)
        input_start(&lb->input_icon);
    if (lb->input_general.eof)
        input_start(&lb->input_general);

    return TRUE;
}


/* Callback when the configuration dialog has recorded a configuration change. */
static void lb_apply_configuration(Plugin * p)
{
    lb_t * lb = (lb_t *) p->priv;

    if (!p->pwid)
        p->pwid = gtk_event_box_new(),
        gtk_widget_show(p->pwid);

    if (!lb->button)
    {
        lb->button = fb_button_new_from_file_with_label(lb->icon_path,
                     p->panel->icon_size, p->panel->icon_size, PANEL_ICON_HIGHLIGHT, TRUE, p->panel, lb->title);
        gtk_container_add(GTK_CONTAINER(p->pwid), lb->button);
        g_signal_connect(lb->button, "button-press-event", G_CALLBACK(lb_press_event), (gpointer) lb);
        gtk_widget_show(lb->button);
    }
    else
    {
        fb_button_set_label(lb->button, p->panel, lb->title);
        fb_button_set_from_file(lb->button, lb->icon_path, p->panel->icon_size, p->panel->icon_size, TRUE);
    }

    fb_button_set_orientation(lb->button, p->panel->orientation);

    if (!strempty(lb->tooltip)) {
        gtk_widget_set_tooltip_text(lb->button, lb->tooltip);
    } else {
        gchar * tooltip = NULL;
        if (strempty(lb->command2) && strempty(lb->command3)) {
            if (strempty(lb->command1))
                tooltip = g_strdup("");
            else
                tooltip = g_strdup_printf(_("%s"), lb->command1);
        } else {
            if (!strempty(lb->command1))
            {
                gchar * t1 = g_strdup_printf(_("Left click: %s"), lb->command1);
                if (tooltip) {
                    gchar * t2 = g_strdup_printf("%s\n%s", tooltip, t1);
                    g_free(t1);
                    g_free(tooltip);
                    tooltip = t2;
                } else {
                    tooltip = t1;
                }
            }
            if (!strempty(lb->command2))
            {
                gchar * t1 = g_strdup_printf(_("Middle click: %s"), lb->command2);
                if (tooltip) {
                    gchar * t2 = g_strdup_printf("%s\n%s", tooltip, t1);
                    g_free(t1);
                    g_free(tooltip);
                    tooltip = t2;
                } else {
                    tooltip = t1;
                }
            }
            if (!strempty(lb->command3))
            {
                gchar * t1 = g_strdup_printf(_("Right click: %s"), lb->command3);
                if (tooltip) {
                    gchar * t2 = g_strdup_printf("%s\n%s", tooltip, t1);
                    g_free(t1);
                    g_free(tooltip);
                    tooltip = t2;
                } else {
                    tooltip = t1;
                }
            }
        }
        gtk_widget_set_tooltip_text(lb->button, tooltip);
        g_free(tooltip);
    }

    if (lb->input_timeout)
    {
        g_source_remove(lb->input_timeout);
        lb->input_timeout = 0;
    }

    if (lb->use_pipes)
    {
        input_start(&lb->input_title);
        input_start(&lb->input_tooltip);
        input_start(&lb->input_icon);
        input_start(&lb->input_general);

        if (lb->input_restart_interval)
            lb->input_timeout = g_timeout_add(lb->input_restart_interval, (GSourceFunc) lb_input_timeout, lb);
    }
    else
    {
        input_stop(&lb->input_title);
        input_stop(&lb->input_tooltip);
        input_stop(&lb->input_icon);
        input_stop(&lb->input_general);
    }
}


/* Plugin constructor. */
static int lb_constructor(Plugin *p, char **fp)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    lb_t * lb = g_new0(lb_t, 1);
    lb->plug = p;
    p->priv = lb;

    lb->input_title.lb = lb;
    lb->input_icon.lb = lb;
    lb->input_tooltip.lb = lb;
    lb->input_general.lb = lb;

    lb->icon_path = NULL;
    lb->title     = NULL;
    lb->tooltip   = NULL;
    lb->command1  = NULL;
    lb->command2  = NULL;
    lb->command3  = NULL;

    lb->button = NULL;
    lb->img    = NULL;
    lb->label  = NULL;

    /* Load parameters from the configuration file. */
    line s;
    s.len = 256;
    if (fp)
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END)
        {
            if (s.type == LINE_NONE)
            {
                ERR( "launchbutton: illegal token %s\n", s.str);
                return 0;
            }
            if (s.type == LINE_VAR)
            {
                if (g_ascii_strcasecmp(s.t[0], "IconPath") == 0)
                    lb->icon_path = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "Title") == 0)
                    lb->title = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "Tooltip") == 0)
                    lb->tooltip = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "Command1") == 0)
                    lb->command1 = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "Command2") == 0)
                    lb->command2 = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "Command3") == 0)
                    lb->command3 = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "InteractiveUpdates") == 0)
                    lb->use_pipes = str2num(bool_pair, s.t[1], lb->use_pipes);
                else if (g_ascii_strcasecmp(s.t[0], "InteractiveUpdateTitle") == 0)
                    lb->input_title.command = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "InteractiveUpdateTooltip") == 0)
                    lb->input_tooltip.command = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "InteractiveUpdateIconPath") == 0)
                    lb->input_icon.command = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "InteractiveUpdateGeneral") == 0)
                    lb->input_general.command = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "InteractiveUpdateRestartInterval") == 0)
                    lb->input_restart_interval = atoi(s.t[1]);
                else
                    ERR( "launchbutton: unknown var %s\n", s.t[0]);
            }
            else
            {
                ERR( "launchbutton: illegal in this context %s\n", s.str);
                return 0;
            }
        }

    }

    #define DEFAULT_STRING(f, v) \
      if (lb->f == NULL) \
          lb->f = g_strdup(v);

    if (!lb->title) {
        //DEFAULT_STRING(icon_path, PACKAGE_DATA_DIR "/lxpanel/images/my-computer.png");
        DEFAULT_STRING(icon_path, "application-x-executable");
    } else {
        DEFAULT_STRING(icon_path, "");
    }
    DEFAULT_STRING(title    , "");
    DEFAULT_STRING(tooltip  , "");
    DEFAULT_STRING(command1 , "");
    DEFAULT_STRING(command2 , "");
    DEFAULT_STRING(command3 , "");

    #undef DEFAULT_STRING

    lb_apply_configuration(p);

    return 1;
}


/* Plugin destructor. */
static void lb_destructor(Plugin * p)
{
    lb_t * lb = (lb_t *) p->priv;

    if (lb->input_timeout)
        g_source_remove(lb->input_timeout);

    input_stop(&lb->input_title);
    input_stop(&lb->input_tooltip);
    input_stop(&lb->input_icon);
    input_stop(&lb->input_general);

    /* Deallocate all memory. */
    g_free(lb->icon_path);
    g_free(lb->title);
    g_free(lb->tooltip);
    g_free(lb->command1);
    g_free(lb->command2);
    g_free(lb->command3);
    g_free(lb->command1_override);
    g_free(lb->command2_override);
    g_free(lb->command3_override);
    g_free(lb->input_title.command);
    g_free(lb->input_tooltip.command);
    g_free(lb->input_icon.command);
    g_free(lb->input_general.command);
    g_free(lb);
}


/* Callback when the configuration dialog is to be shown. */
static void lb_configure(Plugin * p, GtkWindow * parent)
{
    lb_t * lb = (lb_t *) p->priv;

    int min_input_restart_interval = 0;
    int max_input_restart_interval = 100000;
    
    GtkWidget * dlg = create_generic_config_dlg(
        _(p->class->name),
        GTK_WIDGET(parent),
        (GSourceFunc) lb_apply_configuration, (gpointer) p,

        _("General"), (gpointer)NULL, (GType)CONF_TYPE_BEGIN_PAGE,

        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("Title")  , &lb->title    , (GType)CONF_TYPE_STR,
        _("Tooltip"), &lb->tooltip  , (GType)CONF_TYPE_STR,
        _("Icon")   , &lb->icon_path, (GType)CONF_TYPE_FILE_ENTRY,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("Left button command")  , &lb->command1, (GType)CONF_TYPE_STR,
        _("Middle button command"), &lb->command2, (GType)CONF_TYPE_STR,
        _("Right button command") , &lb->command3, (GType)CONF_TYPE_STR,

        _("Interactive updates"), (gpointer)NULL, (GType)CONF_TYPE_BEGIN_PAGE,
        _("Enable interactive updates"), (gpointer)&lb->use_pipes, (GType)CONF_TYPE_BOOL,
        _("Command restart interval"), (gpointer)&lb->input_restart_interval, (GType)CONF_TYPE_INT,
        "int-min-value", (gpointer)&min_input_restart_interval, (GType)CONF_TYPE_SET_PROPERTY,
        "int-max-value", (gpointer)&max_input_restart_interval, (GType)CONF_TYPE_SET_PROPERTY,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("Title update command")  , &lb->input_title.command, (GType)CONF_TYPE_STR,
        _("Tooltip  update command"), &lb->input_tooltip.command, (GType)CONF_TYPE_STR,
        _("Icon path update command"), &lb->input_icon.command, (GType)CONF_TYPE_STR,
        _("General update command"), &lb->input_general.command, (GType)CONF_TYPE_STR,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,

        NULL);

    if (dlg)
        gtk_window_present(GTK_WINDOW(dlg));
}


/* Callback when the configuration is to be saved. */
static void lb_save_configuration(Plugin * p, FILE * fp)
{
    lb_t * lb = (lb_t *) p->priv;
    lxpanel_put_str(fp, "IconPath", lb->icon_path);
    lxpanel_put_str(fp, "Title", lb->title);
    lxpanel_put_str(fp, "Tooltip", lb->tooltip);
    lxpanel_put_str(fp, "Command1", lb->command1);
    lxpanel_put_str(fp, "Command2", lb->command2);
    lxpanel_put_str(fp, "Command3", lb->command3);

    lxpanel_put_bool(fp, "InteractiveUpdates", lb->use_pipes);
    lxpanel_put_int(fp, "InteractiveUpdateRestartInterval", lb->input_restart_interval);
    lxpanel_put_str(fp, "InteractiveUpdateTitle", lb->input_title.command);
    lxpanel_put_str(fp, "InteractiveUpdateTooltip", lb->input_tooltip.command);
    lxpanel_put_str(fp, "InteractiveUpdateIconPath", lb->input_icon.command);
    lxpanel_put_str(fp, "InteractiveUpdateRestartInterval", lb->input_general.command);
}


/* Callback when panel configuration changes. */
static void lb_panel_configuration_changed(Plugin * p)
{
    lb_apply_configuration(p);
}


PluginClass launchbutton_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "launchbutton",
    name : N_("Button"),
    version: "0.1",
    description : N_("Launch button"),

    constructor : lb_constructor,
    destructor  : lb_destructor,
    config : lb_configure,
    save : lb_save_configuration,
    panel_configuration_changed : lb_panel_configuration_changed
};

