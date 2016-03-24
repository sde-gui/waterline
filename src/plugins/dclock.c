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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <sde-utils-jansson.h>

#define PLUGIN_PRIV_TYPE DClockPlugin

#include <waterline/symbol_visibility.h>
#include <waterline/paths.h>
#include <waterline/panel.h>
#include <waterline/misc.h>
#include <waterline/plugin.h>
#include <waterline/gtkcompat.h>

#define DEFAULT_TIP_FORMAT    "%A %x"
#define DEFAULT_CLOCK_FORMAT  "%R"

#define MAX_LABELS 4

/* Private context for digital clock plugin. */
typedef struct {
    Plugin * plugin;
    GtkWidget * label_box;
    GtkWidget * clock_labels[MAX_LABELS];
    GtkWidget * calendar_window;

    char * clock_format;            /* Format string for clock value */
    char * tooltip_format;          /* Format string for tooltip value */
    char * action;                  /* Command to execute on a click */
    char * timezone;                /* Timezone */
    char * font;
    guint timer;                    /* Timer for periodic update */
    enum {
        AWAITING_FIRST_CHANGE,      /* Experimenting to determine interval, waiting for first change */
        AWAITING_SECOND_CHANGE,     /* Experimenting to determine interval, waiting for second change */
        ONE_SECOND_INTERVAL,        /* Determined that one second interval is necessary */
        ONE_MINUTE_INTERVAL         /* Determined that one minute interval is sufficient */
    } expiration_interval;
    int experiment_count;           /* Count of experiments that have been done to determine interval */
    char * prev_clock_value;        /* Previous value of clock */
    char * prev_tooltip_value;      /* Previous value of tooltip */
    char * timezones;
} DClockPlugin;

static void dclock_popup_map(GtkWidget * widget, DClockPlugin * dc);
static GtkWidget * dclock_create_calendar(DClockPlugin * dc);
static gboolean dclock_button_press_event(GtkWidget * widget, GdkEventButton * evt, Plugin * plugin);
static void dclock_timer_set(DClockPlugin * dc);
static gboolean dclock_update_display(DClockPlugin * dc);
static void dclock_apply_configuration(Plugin * p);
static void dclock_panel_configuration_changed(Plugin * p);

/******************************************************************************/

#define SU_JSON_OPTION_STRUCTURE DClockPlugin
static su_json_option_definition option_definitions[] = {
    SU_JSON_OPTION(string, clock_format),
    SU_JSON_OPTION(string, tooltip_format),
    SU_JSON_OPTION(string, action),
    SU_JSON_OPTION(string, font),
    SU_JSON_OPTION(string, timezone),
    {0,}
};

/******************************************************************************/

static gchar ** dclock_get_format_strings(Plugin * plugin)
{
    gchar ** result = NULL;
    gchar * filename = NULL;
    gchar * contents = NULL;

    filename = plugin_get_config_path(plugin, "formats", SU_PATH_CONFIG_USER);
    if (!filename)
        goto ret;

    gboolean ok = g_file_get_contents(filename, &contents, NULL, NULL);
    if (!ok || !contents)
        goto ret;

    result = g_strsplit_set(contents, "\n\r", 0);

ret:
   g_free(contents); 
   g_free(filename); 
   return result;
}

static void dclock_copy_to_clipboard_menu_item_activate(GtkMenuItem *item)
{
    GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, gtk_menu_item_get_label(item), -1);
}

static void dclock_generate_copy_to_clipboard_menu(GtkMenu* panel_menu, Plugin * plugin)
{
    time_t now;
    time(&now);

    struct tm * current_time;
    current_time = localtime(&now);

    gchar ** formats = dclock_get_format_strings(plugin);
    if(!formats)
        return;

    GtkWidget * menu = gtk_menu_new();
    gboolean empty = TRUE;

    int i;
    for (i = 0; formats[i]; i++)
    {
        gchar * format = g_strstrip(formats[i]);
        if (format[0] == 0 || format[0] == '#')
            continue;

        if (strcmp(format, "-") == 0)
        {
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
            continue;
        }

        char buf[128];
        strftime(buf, 128, format, current_time);
        GtkWidget* item = gtk_menu_item_new_with_label(buf);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(dclock_copy_to_clipboard_menu_item_activate), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

        empty = FALSE;
    }

    g_strfreev(formats);

    if (empty)
    {
        g_object_ref_sink(G_OBJECT(menu));
        g_object_unref(G_OBJECT(menu));
    }
    else
    {
        gtk_menu_shell_prepend(GTK_MENU_SHELL(panel_menu), wtl_gtk_widget_show(gtk_separator_menu_item_new()));

        GtkWidget * copy_to_clipboard = gtk_menu_item_new_with_label(_("Copy to Clipboard..."));
        gtk_menu_shell_prepend(GTK_MENU_SHELL(panel_menu), copy_to_clipboard);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(copy_to_clipboard), menu);
        gtk_widget_show(copy_to_clipboard);
        gtk_widget_show_all(menu);
    }
}

static char * dclock_get_timezones(DClockPlugin * dc)
{
    if (!dc->timezones)
    {
        FILE * fpipe;
        char * command = "grep -v '^#' /usr/share/zoneinfo/zone.tab | awk '{print $3}' | sort";

        if ( !(fpipe = popen(command,"r")) )
        {
            return NULL;
        }

        GIOChannel * channel = g_io_channel_unix_new(fileno(fpipe));
        if (channel)
        {
            gchar * data;
            gsize data_size;
            GIOStatus status = g_io_channel_read_to_end(channel, &data, &data_size, NULL);
            if (status == G_IO_STATUS_NORMAL)
            {
                dc->timezones = g_strdup_printf("\n%s", data);
                g_free(data);
            }
            g_io_channel_unref(channel);
        }

        pclose(fpipe);
    }

    return dc->timezones;
}

/* Handler for "map" signal on popup window. */
static void dclock_popup_map(GtkWidget * widget, DClockPlugin * dc)
{
    plugin_adjust_popup_position(widget, dc->plugin);
}

/* Display a window containing the standard calendar widget. */
static GtkWidget * dclock_create_calendar(DClockPlugin * dc)
{
    /* Create a new window. */
    GtkWindow * window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_skip_taskbar_hint(window, TRUE);
    gtk_window_set_skip_pager_hint(window, TRUE);
    gtk_window_set_default_size(window, 180, 180);
    gtk_window_set_decorated(window, FALSE);
    gtk_window_set_resizable(window, FALSE);
    gtk_window_stick(window);
    gtk_container_set_border_width(GTK_CONTAINER(window), 0);

    GtkWidget * viewport = gtk_viewport_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(window), viewport);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_OUT);
    gtk_widget_show(viewport);

    /* Create a vertical box as a child of the window. */
    GtkWidget * box = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(viewport), GTK_WIDGET(box));

    /* Create a standard calendar widget as a child of the vertical box. */
    GtkWidget * calendar = gtk_calendar_new();
    gtk_calendar_set_display_options(
        GTK_CALENDAR(calendar),
        GTK_CALENDAR_SHOW_WEEK_NUMBERS | GTK_CALENDAR_SHOW_DAY_NAMES | GTK_CALENDAR_SHOW_HEADING);
    gtk_box_pack_start(GTK_BOX(box), calendar, TRUE, TRUE, 0);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(window), "map", G_CALLBACK(dclock_popup_map), dc);

    /* Return the widget. */
    return GTK_WIDGET(window);
}

static void dclock_show_calendar(DClockPlugin * dc)
{
    if (dc->calendar_window == NULL)
    {
        dc->calendar_window = dclock_create_calendar(dc);
    }
    plugin_adjust_popup_position(dc->calendar_window, dc->plugin);
    gtk_widget_show_all(dc->calendar_window);
}

static void dclock_hide_calendar(DClockPlugin * dc)
{
    if (dc->calendar_window != NULL)
    {
        gtk_widget_destroy(dc->calendar_window);
        dc->calendar_window = NULL;
    }
}

/* Handler for "button-press-event" event from main widget. */
static gboolean dclock_button_press_event(GtkWidget * widget, GdkEventButton * evt, Plugin * plugin)
{
    DClockPlugin * dc = PRIV(plugin);

    if (plugin_button_press_event(widget, evt, plugin))
        return TRUE;

    /* If an action is set, execute it. */
    if (!su_str_empty(dc->action))
    {
        g_spawn_command_line_async(dc->action, NULL);
    }
    /* If no action is set, toggle the presentation of the calendar. */
    else
    {
        if (dc->calendar_window == NULL)
        {
            dclock_show_calendar(dc);
        }
        else
        {
            dclock_hide_calendar(dc);
        }
    }
    return TRUE;
}

/* Set the timer. */
static void dclock_timer_set(DClockPlugin * dc)
{
    int milliseconds = 1000;

    /* Get current time to millisecond resolution. */
    struct timeval current_time;
    if (gettimeofday(&current_time, NULL) >= 0)
    {
        /* Compute number of milliseconds until next second boundary. */
        milliseconds = 1000 - (current_time.tv_usec / 1000);

        /* If the expiration interval is the minute boundary,
         * add number of milliseconds after that until next minute boundary. */
        if (dc->expiration_interval == ONE_MINUTE_INTERVAL)
        {
            time_t seconds = 60 - (current_time.tv_sec - (current_time.tv_sec / 60) * 60);
            milliseconds += seconds * 1000;
        }
    }

    /* Be defensive, and set the timer. */
    if (milliseconds <= 0)
        milliseconds = 1000;
    dc->timer = g_timeout_add(milliseconds, (GSourceFunc) dclock_update_display, (gpointer) dc);
}

static struct tm * _get_time_to_display(DClockPlugin * dc)
{
    time_t now;
    time(&now);

    struct tm * current_time = NULL;
    gchar* oldtz = NULL;

    if (!su_str_empty(dc->timezone)) {
        oldtz = g_strdup(g_getenv("TZ"));
        g_setenv("TZ", dc->timezone, 1);
        current_time = localtime(&now);
        if (oldtz)
            g_setenv("TZ", oldtz, 1);
        else
            g_unsetenv("TZ");
    } else
        current_time = localtime(&now);

    return current_time;
}

/* Periodic timer callback.
 * Also used during initialization and configuration change to do a redraw. */
static gboolean dclock_update_display(DClockPlugin * dc)
{
    /* Determine the current time. */
    struct tm * current_time = _get_time_to_display(dc);

    /* Determine the content of the clock label and tooltip. */
    char clock_value[256];
    char tooltip_value[256];

    clock_value[0] = '\0';
    if (dc->clock_format != NULL)
        strftime(clock_value, sizeof(clock_value), dc->clock_format, current_time);
    tooltip_value[0] = '\0';
    if (dc->tooltip_format != NULL)
        strftime(tooltip_value, sizeof(tooltip_value), dc->tooltip_format, current_time);
    /* When we write the clock value, it causes the panel to do a full relayout.
     * Since this function may be called too often while the timing experiment is underway,
     * we take the trouble to check if the string actually changed first. */
    if ((dc->prev_clock_value == NULL) || (strcmp(dc->prev_clock_value, clock_value) != 0))
    {
        /* Convert "\n" escapes in the user's format string to newline characters. */
        char * newlines_converted = NULL;
        if (strstr(clock_value, "\\n") != NULL)
        {
            newlines_converted = g_strdup(clock_value); /* Just to get enough space for the converted result */
            char * p;
            char * q;
            for (p = clock_value, q = newlines_converted; *p != '\0'; p += 1)
            {
                if ((p[0] == '\\') && (p[1] == 'n'))
                {
                    *q++ = '\n';
                    p += 1;
                }
                else
                    *q++ = *p;
            }
            *q = '\0';
        }

        gchar ** segments = g_strsplit(
            (newlines_converted != NULL) ? newlines_converted : clock_value,
            "\\t", MAX_LABELS);

        int i;
        for (i = 0; i < MAX_LABELS; i++)
        {
            if (!segments || !segments[i])
                break;
            if (!dc->clock_labels[i])
            {
                dc->clock_labels[i] = gtk_label_new(NULL);
                gtk_misc_set_alignment(GTK_MISC(dc->clock_labels[i]), 0.5, 0.5);
                gtk_container_add(GTK_CONTAINER(dc->label_box), dc->clock_labels[i]);
                gtk_label_set_justify(GTK_LABEL(dc->clock_labels[i]), GTK_JUSTIFY_CENTER);
            }
            panel_draw_label_text_with_font(
                plugin_panel(dc->plugin), dc->clock_labels[i],
                segments[i],
                STYLE_CUSTOM_COLOR | STYLE_MARKUP, dc->font);
            gtk_widget_show(dc->clock_labels[i]);
        }

        for (/*nothing*/; i < MAX_LABELS; i++)
        {
            if (!dc->clock_labels[i])
                break;
            gtk_widget_hide(dc->clock_labels[i]);
        }

        g_strfreev(segments);

        g_free(newlines_converted);
    }

    /* Determine the content of the tooltip. */
    gtk_widget_set_tooltip_text(plugin_widget(dc->plugin), tooltip_value);

    /* Conduct an experiment to see how often the value changes.
     * Use this to decide whether we update the value every second or every minute.
     * We need to account for the possibility that the experiment is being run when we cross a minute boundary. */
    if (dc->expiration_interval < ONE_SECOND_INTERVAL)
    {
        if (dc->prev_clock_value == NULL)
        {
            /* Initiate the experiment. */
            dc->prev_clock_value = g_strdup(clock_value);
            dc->prev_tooltip_value = g_strdup(tooltip_value);
        }
        else
        {
            if ((strcmp(dc->prev_clock_value, clock_value) == 0)
            && (strcmp(dc->prev_tooltip_value, tooltip_value) == 0))
            {
                dc->experiment_count += 1;
                if (dc->experiment_count > 3)
                {
                    /* No change within 3 seconds.  Assume change no more often than once per minute. */
                    dc->expiration_interval = ONE_MINUTE_INTERVAL;
                    g_free(dc->prev_clock_value);
                    g_free(dc->prev_tooltip_value);
                    dc->prev_clock_value = NULL;
                    dc->prev_tooltip_value = NULL;
                }
            }
            else if (dc->expiration_interval == AWAITING_FIRST_CHANGE)
            {
                /* We have a change at the beginning of the experiment, but we do not know when the next change might occur.
                 * Continue the experiment for 3 more seconds. */
                dc->expiration_interval = AWAITING_SECOND_CHANGE;
                dc->experiment_count = 0;
                g_free(dc->prev_clock_value);
                g_free(dc->prev_tooltip_value);
                dc->prev_clock_value = g_strdup(clock_value);
                dc->prev_tooltip_value = g_strdup(tooltip_value);
            }
            else
            {
                /* We have a second change.  End the experiment. */
                dc->expiration_interval = ((dc->experiment_count > 3) ? ONE_MINUTE_INTERVAL : ONE_SECOND_INTERVAL);
                g_free(dc->prev_clock_value);
                g_free(dc->prev_tooltip_value);
                dc->prev_clock_value = NULL;
                dc->prev_tooltip_value = NULL;
            }
        }
    }

    /* Reset the timer and return. */
    dclock_timer_set(dc);
    return FALSE;
}

/* Plugin constructor. */
static int dclock_constructor(Plugin * p)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    DClockPlugin * dc = g_new0(DClockPlugin, 1);
    plugin_set_priv(p, dc);
    dc->plugin = p;

    /* Initialize the clock display. */
    dc->clock_format = g_strdup(_(DEFAULT_CLOCK_FORMAT));
    dc->tooltip_format = g_strdup(_(DEFAULT_TIP_FORMAT));

    su_json_read_options(plugin_inner_json(p), option_definitions, dc);

    /* Allocate top level widget and set into Plugin widget pointer. */
    GtkWidget * pwid = gtk_event_box_new();
    plugin_set_widget(p, pwid);
    gtk_widget_set_has_window(pwid, FALSE);

    GtkWidget * hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(pwid), hbox);
    gtk_widget_show(hbox);

    dc->label_box = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(hbox), dc->label_box);

    /* Connect signals. */
    g_signal_connect(G_OBJECT (pwid), "button_press_event", G_CALLBACK(dclock_button_press_event), (gpointer) p);

    dclock_apply_configuration(p);

    /* Show the widget and return. */
    gtk_widget_show(pwid);
    return 1;
}

/* Plugin destructor. */
static void dclock_destructor(Plugin * p)
{
    DClockPlugin * dc = PRIV(p);

    /* Remove the timer. */
    if (dc->timer != 0)
        g_source_remove(dc->timer);

    /* Ensure that the calendar is dismissed. */
    if (dc->calendar_window != NULL)
        gtk_widget_destroy(dc->calendar_window);

    /* Deallocate all memory. */
    g_free(dc->clock_format);
    g_free(dc->tooltip_format);
    g_free(dc->action);
    g_free(dc->font);
    g_free(dc->prev_clock_value);
    g_free(dc->prev_tooltip_value);
    g_free(dc->timezone);
    g_free(dc->timezones);
    g_free(dc);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static void dclock_apply_configuration(Plugin * p)
{
    DClockPlugin * dc = PRIV(p);

    gtk_widget_show(dc->label_box);

    /* Rerun the experiment to determine update interval and update the display. */
    g_free(dc->prev_clock_value);
    g_free(dc->prev_tooltip_value);
    dc->expiration_interval = AWAITING_FIRST_CHANGE;
    dc->experiment_count = 0;
    dc->prev_clock_value = NULL;
    dc->prev_tooltip_value = NULL;

    /* Remove the timer before calling dclock_update_display(),
       as dclock_timer_set() overwrites dc->timer without removing old one. */
    if (dc->timer != 0)
        g_source_remove(dc->timer);

    dclock_update_display(dc);

    /* Hide the calendar. */
    if (dc->calendar_window != NULL)
    {
        gtk_widget_destroy(dc->calendar_window);
        dc->calendar_window = NULL;
    }
}

/* Callback when the configuration dialog is to be shown. */
static void dclock_configure(Plugin * p, GtkWindow * parent)
{
    DClockPlugin * dc = PRIV(p);

    const char * clock_format_tooltip = _(
        "See the manual page strftime(3) for possible format codes.\n\n"
        "You can use the Pango markup to adjust text color, font size and other text properties.\n\n"
        "Additionaly you can use:\n"
        "\\n for line break\n"
        "\\t for column break"
    );

    const char * tooltip_format_tooltip = _(
        "See the manual page strftime(3) for possible format codes.\n\n"
        "You can use the Pango markup to adjust text color, font size and other text properties."
    );

    GtkWidget * dialog = wtl_create_generic_config_dialog(
        _(plugin_class(p)->name),
        GTK_WIDGET(parent),
        (GSourceFunc) dclock_apply_configuration, (gpointer) p,

        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("Clock Format")  , &dc->clock_format  , (GType)CONF_TYPE_STR,
        "tooltip-text", clock_format_tooltip, (GType)CONF_TYPE_SET_PROPERTY,
        _("Tooltip Format"), &dc->tooltip_format, (GType)CONF_TYPE_STR,
        "tooltip-text", tooltip_format_tooltip, (GType)CONF_TYPE_SET_PROPERTY,
        _("Font")          , &dc->font          , (GType)CONF_TYPE_STR,
        _("Action when clicked"), &dc->action, (GType)CONF_TYPE_STR,
        "tooltip-text", _("Default action: display calendar"), (GType)CONF_TYPE_SET_PROPERTY,
        _("Timezone")  , &dc->timezone , (GType)CONF_TYPE_STR,
        "completion-list", (gpointer)dclock_get_timezones(dc), (GType)CONF_TYPE_SET_PROPERTY,
        "", 0, (GType)CONF_TYPE_END_TABLE,
        NULL);
    if (dialog)
        gtk_window_present(GTK_WINDOW(dialog));
}

/* Callback when the configuration is to be saved. */
static void dclock_save_configuration(Plugin * p)
{
    DClockPlugin * dc = PRIV(p);
    su_json_write_options(plugin_inner_json(p), option_definitions, dc);
}

/* Callback when panel configuration changes. */
static void dclock_panel_configuration_changed(Plugin * p)
{
    dclock_apply_configuration(p);
}

static void dclock_run_command_calendar_visible(Plugin * p, char ** argv, int argc)
{
    DClockPlugin * dc = PRIV(p);

    gboolean visible = dc->calendar_window != NULL;
    gboolean old_visible = visible;
    visible = !visible;

    if (argc >= 1)
    {
        if (strcmp(argv[0], "true") == 0 || strcmp(argv[0], "1") == 0)
            visible = TRUE;
        else if (strcmp(argv[0], "false") == 0 || strcmp(argv[0], "0") == 0)
            visible = FALSE;
    }

    if (visible != old_visible)
    {
        if (visible)
            dclock_show_calendar(dc);
        else
            dclock_hide_calendar(dc);
    }
}

static void dclock_run_command_calendar(Plugin * p, char ** argv, int argc)
{
    if (argc < 1)
        return;

    if (strcmp(argv[0], "visible") == 0)
    {
        dclock_run_command_calendar_visible(p, argv + 1, argc - 1);
    }
}

static void dclock_run_command(Plugin * p, char ** argv, int argc)
{
    if (argc < 1)
        return;

    if (strcmp(argv[0], "calendar") == 0)
    {
        dclock_run_command_calendar(p, argv + 1, argc - 1);
    }
}

static void dclock_popup_menu_hook(struct _Plugin * plugin, GtkMenu * menu)
{
    dclock_generate_copy_to_clipboard_menu(menu, plugin);
}

/* Plugin descriptor. */
SYMBOL_PLUGIN_CLASS PluginClass dclock_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "dclock",
    name : N_("Digital Clock"),
    version: VERSION,
    description : N_("Display digital clock and tooltip"),
    category: PLUGIN_CATEGORY_HW_INDICATOR,

    constructor : dclock_constructor,
    destructor  : dclock_destructor,
    show_properties : dclock_configure,
    save_configuration : dclock_save_configuration,
    panel_configuration_changed : dclock_panel_configuration_changed,
    run_command : dclock_run_command,
    popup_menu_hook : dclock_popup_menu_hook
};
