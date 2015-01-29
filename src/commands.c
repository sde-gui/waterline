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

#include "commands.h"
#include <waterline/global.h>
#include <waterline/defaultapplications.h>
#include <waterline/panel.h>
#include <waterline/plugin.h>
#include <waterline/misc.h>
#include "bg.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <glib/gi18n.h>

#include <waterline/dbg.h>

void restart(void);
void gtk_run(void);
static void logout(void);

Command commands[] = {
#ifndef DISABLE_MENU
    { "run", N_("Run"), gtk_run },
#endif
    { "restart", N_("Restart"), restart },
    { "logout", N_("Logout"), logout },
    { NULL, NULL },
};


void restart(void)
{
    /* This is defined in panel.c */
    extern gboolean is_restarting;
    ENTER;
    is_restarting = TRUE;

    gtk_main_quit();
    RET();
}

void logout(void)
{
    const char* l_logout_cmd = wtl_get_logout_command();
    /* If LXSession is running, _LXSESSION_PID will be set */
    if( ! l_logout_cmd && getenv("_LXSESSION_PID") )
        l_logout_cmd = "lxsession-logout";

    if( l_logout_cmd ) {
        GError* err = NULL;
        if( ! g_spawn_command_line_async( l_logout_cmd, &err ) ) {
            show_error( NULL, err->message );
            g_error_free( err );
        }
    }
    else {
        show_error( NULL, _("Logout command is not set") );
    }
}

