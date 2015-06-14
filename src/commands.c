/**
 *
 * Copyright (c) 2011 Vadim Ushakov
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

#include "wtl_private.h"
#include <waterline/global.h>
#include <waterline/defaultapplications.h>
#include <waterline/misc.h>
#include <waterline/commands.h>
#include "bg.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <glib/gi18n.h>

typedef struct {
    char * name;
    char * disp_name;
    void ( * cmd)(void);
} Command;

static void logout(void);

static Command commands[] = {
#ifndef DISABLE_MENU
    { "run", N_("Run"), wtl_show_run_box },
#endif
    { "restart", N_("Restart"), wtl_restart },
    { "logout", N_("Logout"), logout },
    { NULL, NULL },
};

const char * wtl_command_get_const_name(const char * command_name)
{
    Command * tmp = NULL;
    for (tmp = commands; tmp->name; tmp++)
    {
        if (g_strcmp0(command_name, tmp->name) == 0)
        {
            return tmp->name;
        }
    }
    return NULL;
}

const char * wtl_command_get_displayed_name(const char * command_name)
{
    Command * tmp = NULL;
    for (tmp = commands; tmp->name; tmp++)
    {
        if (g_strcmp0(command_name, tmp->name) == 0)
        {
            return _(tmp->disp_name);
        }
    }
    return NULL;
}

gboolean wtl_command_exists(const char * command_name)
{
    Command * tmp = NULL;
    for (tmp = commands; tmp->name; tmp++)
    {
        if (g_strcmp0(command_name, tmp->name) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

void wtl_command_run(const char * command_name)
{
    Command * tmp = NULL;
    for (tmp = commands; tmp->name; tmp++)
    {
        if (g_strcmp0(command_name, tmp->name) == 0)
        {
            tmp->cmd();
            break;
        }
    }
}


void wtl_restart(void)
{
    /* This is defined in panel.c */
    extern gboolean is_restarting;
    is_restarting = TRUE;
    gtk_main_quit();
}

void logout(void)
{
    const char * logout_command = wtl_get_logout_command();
    if (logout_command) {
        GError* err = NULL;
        if (!g_spawn_command_line_async(logout_command, &err)) {
            show_error(NULL, err->message);
            g_error_free(err);
        }
    }
    else {
        show_error(NULL, _("Logout command is not set"));
    }
}

