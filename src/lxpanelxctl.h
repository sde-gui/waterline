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

#ifndef _LXPANELXCTL_H
#define _LXPANELXCTL_H

/* Commands controlling lxpanelx.
 * These are the parameter of a _LXPANELX_CMD ClientMessage to the root window.
 * Endianness alert:  Note that the parameter is in b[0], not l[0]. */
typedef enum {
    LXPANELX_CMD_NONE,
    LXPANELX_CMD_SYS_MENU,
    LXPANELX_CMD_RUN,
    LXPANELX_CMD_CONFIG,
    LXPANELX_CMD_RESTART,
    LXPANELX_CMD_EXIT
} PanelControlCommand;

#endif
