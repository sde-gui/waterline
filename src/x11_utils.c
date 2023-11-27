/**
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

#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <string.h>
#include <sde-utils-gtk.h>
#include <sde-utils.h>
#include <waterline/x11_utils.h>
#include <waterline/x11_wrappers.h>
#include "wtl_private.h"

void * wtl_x11_get_xa_property(Window xid, Atom prop, Atom type, int * nitems)
{
    return su_x11_get_xa_property(wtl_x11_display(), xid, prop, type, nitems);
}

char * wtl_x11_get_utf8_property(Window win, Atom atom)
{
    return su_x11_get_utf8_property(wtl_x11_display(), win, atom);
}


char ** wtl_x11_get_utf8_property_list(Window win, Atom atom, int *count)
{
    Atom type;
    int format, i;
    gulong nitems;
    gulong bytes_after;
    gchar *s, **retval = NULL;
    int result;
    guchar *tmp = NULL;

    *count = 0;
    result = XGetWindowProperty(wtl_x11_display(), win, atom, 0, G_MAXLONG, False,
          aUTF8_STRING, &type, &format, &nitems,
          &bytes_after, &tmp);
    if (result != Success || type != aUTF8_STRING || tmp == NULL)
        return NULL;

    if (nitems) {
        gchar *val = (gchar *) tmp;
        su_log_debug("res=%d(%d) nitems=%d val=%s\n", result, Success, nitems, val);
        for (i = 0; i < nitems; i++) {
            if (!val[i])
                (*count)++;
        }
        retval = g_new0 (char*, *count + 2);
        for (i = 0, s = val; i < *count; i++, s = s +  strlen (s) + 1) {
            retval[i] = g_strdup(s);
        }
        if (val[nitems-1]) {
            result = nitems - (s - val);
            su_log_debug("val does not ends by 0, moving last %d bytes\n", result);
            g_memmove(s - 1, s, result);
            val[nitems-1] = 0;
            su_log_debug("s=%s\n", s -1);
            retval[i] = g_strdup(s - 1);
            (*count)++;
        }
    }
    XFree (tmp);

    return retval;

}

static char * text_property_to_utf8 (const XTextProperty *prop)
{
  char **list;
  int count;
  char *retval;

  list = NULL;
  count = gdk_text_property_to_utf8_list (gdk_x11_xatom_to_atom (prop->encoding),
                                          prop->format,
                                          prop->value,
                                          prop->nitems,
                                          &list);

  su_log_debug("count=%d\n", count);
  if (count == 0)
    return NULL;

  retval = list[0];
  list[0] = g_strdup (""); /* something to free */

  g_strfreev (list);

  return retval;
}

char * wtl_x11_get_text_property(Window win, Atom atom)
{
    XTextProperty text_prop;
    char *retval;

    if (XGetTextProperty(wtl_x11_display(), win, &text_prop, atom)) {
        su_log_debug("format=%d enc=%d nitems=%d value=%s   \n",
              text_prop.format,
              text_prop.encoding,
              text_prop.nitems,
              text_prop.value);
        retval = text_property_to_utf8 (&text_prop);
        if (text_prop.nitems > 0)
            XFree (text_prop.value);
        return retval;

    }
    return NULL;
}

/****************************************************************************/

void
Xclimsg(Window win, Atom type, long l0, long l1, long l2, long l3, long l4)
{
    XClientMessageEvent xev;
    xev.type = ClientMessage;
    xev.window = win;
    xev.message_type = type;
    xev.format = 32;
    xev.data.l[0] = l0;
    xev.data.l[1] = l1;
    xev.data.l[2] = l2;
    xev.data.l[3] = l3;
    xev.data.l[4] = l4;
    XSendEvent(wtl_x11_display(), wtl_x11_root(), False,
          (SubstructureNotifyMask | SubstructureRedirectMask),
          (XEvent *) &xev);
}

void
Xclimsgwm(Window win, Atom type, Atom arg)
{
    XClientMessageEvent xev;

    xev.type = ClientMessage;
    xev.window = win;
    xev.message_type = type;
    xev.format = 32;
    xev.data.l[0] = arg;
    xev.data.l[1] = GDK_CURRENT_TIME;
    XSendEvent(wtl_x11_display(), win, False, 0L, (XEvent *) &xev);
}

int wtl_x11_get_net_number_of_desktops(void)
{
    int number_of_desktops = 0;
    guint32 * data = wtl_x11_get_xa_property (wtl_x11_root(), a_NET_NUMBER_OF_DESKTOPS, XA_CARDINAL, 0);
    if (data)
    {
        number_of_desktops = *data;
        XFree (data);
    }
    return number_of_desktops;
}

int wtl_x11_get_net_current_desktop(void)
{
    int current_desktop = 0;
    guint32 * data = wtl_x11_get_xa_property (wtl_x11_root(), a_NET_CURRENT_DESKTOP, XA_CARDINAL, 0);
    if (data)
    {
        current_desktop = *data;
        XFree (data);
    }
    return current_desktop;
}

int wtl_x11_get_net_wm_desktop(Window win)
{
    int desk = 0;
    guint32 *data;

    data = wtl_x11_get_xa_property (win, a_NET_WM_DESKTOP, XA_CARDINAL, 0);
    if (data) {
        desk = *data;
        XFree (data);
    }
    return desk;
}

void wtl_x11_set_net_wm_desktop(Window win, int num)
{
    Xclimsg(win, a_NET_WM_DESKTOP, num, 0, 0, 0, 0);
}

void wtl_x11_get_net_wm_state(Window win, NetWMState *nws)
{
    Atom *state;
    int num3;

    memset(nws, 0, sizeof(*nws));
    if (!(state = wtl_x11_get_xa_property(win, a_NET_WM_STATE, XA_ATOM, &num3)))
        return;

    SU_LOG_DEBUG2( "%x: netwm state = { ", (unsigned int)win);
    while (--num3 >= 0) {

        if (state[num3] == a_NET_WM_STATE_SKIP_PAGER) {
            SU_LOG_DEBUG2("NET_WM_STATE_SKIP_PAGER ");
            nws->skip_pager = 1;
        } else if (state[num3] == a_NET_WM_STATE_SKIP_TASKBAR) {
            SU_LOG_DEBUG2( "NET_WM_STATE_SKIP_TASKBAR ");
            nws->skip_taskbar = 1;
        } else if (state[num3] == a_NET_WM_STATE_STICKY) {
            SU_LOG_DEBUG2( "NET_WM_STATE_STICKY ");
            nws->sticky = 1;
        } else if (state[num3] == a_NET_WM_STATE_HIDDEN) {
            SU_LOG_DEBUG2( "NET_WM_STATE_HIDDEN ");
            nws->hidden = 1;
        } else if (state[num3] == a_NET_WM_STATE_SHADED) {
            SU_LOG_DEBUG2( "NET_WM_STATE_SHADED ");
            nws->shaded = 1;
        } else if (state[num3] == a_NET_WM_STATE_MODAL) {
            SU_LOG_DEBUG2( "NET_WM_STATE_MODAL ");
            nws->modal = 1;
        } else if (state[num3] == a_NET_WM_STATE_MAXIMIZED_VERT) {
            SU_LOG_DEBUG2( "NET_WM_STATE_MAXIMIZED_VERT ");
            nws->maximized_vert = 1;
        } else if (state[num3] == a_NET_WM_STATE_MAXIMIZED_HORZ) {
            SU_LOG_DEBUG2( "NET_WM_STATE_MAXIMIZED_HORZ ");
            nws->maximized_horz = 1;
        } else if (state[num3] == a_NET_WM_STATE_FULLSCREEN) {
            SU_LOG_DEBUG2( "NET_WM_STATE_FULLSCREEN; ");
            nws->fullscreen = 1;
        } else if (state[num3] == a_NET_WM_STATE_ABOVE) {
            SU_LOG_DEBUG2( "NET_WM_STATE_ABOVE ");
            nws->above = 1;
        } else if (state[num3] == a_NET_WM_STATE_BELOW) {
            SU_LOG_DEBUG2( "NET_WM_STATE_BELOW ");
            nws->below = 1;
        } else if (state[num3] == a_NET_WM_STATE_DEMANDS_ATTENTION) {
            SU_LOG_DEBUG2( "NET_WM_STATE_DEMANDS_ATTENTION ");
            nws->demands_attention = 1;
        } else if (state[num3] == a_OB_WM_STATE_UNDECORATED) {
            SU_LOG_DEBUG2( "OB_WM_STATE_UNDECORATED ");
            nws->ob_undecorated = 1;
        } else {
            SU_LOG_DEBUG2( "... ");
        }
    }
    XFree(state);
    SU_LOG_DEBUG2( "}\n");
}

void wtl_x11_get_net_wm_window_type(Window win, NetWMWindowType *nwwt)
{
    Atom *state;
    int num3;

    memset(nwwt, 0, sizeof(*nwwt));
    if (!(state = wtl_x11_get_xa_property(win, a_NET_WM_WINDOW_TYPE, XA_ATOM, &num3)))
        return;

    SU_LOG_DEBUG2( "%x: netwm state = { ", (unsigned int)win);
    while (--num3 >= 0)
    {
        if (state[num3] == a_NET_WM_WINDOW_TYPE_DESKTOP) {
            SU_LOG_DEBUG2("NET_WM_WINDOW_TYPE_DESKTOP ");
            nwwt->desktop = 1;
        } else if (state[num3] == a_NET_WM_WINDOW_TYPE_DOCK) {
            SU_LOG_DEBUG2( "NET_WM_WINDOW_TYPE_DOCK ");
            nwwt->dock = 1;
        } else if (state[num3] == a_NET_WM_WINDOW_TYPE_TOOLBAR) {
            SU_LOG_DEBUG2( "NET_WM_WINDOW_TYPE_TOOLBAR ");
            nwwt->toolbar = 1;
        } else if (state[num3] == a_NET_WM_WINDOW_TYPE_MENU) {
            SU_LOG_DEBUG2( "NET_WM_WINDOW_TYPE_MENU ");
            nwwt->menu = 1;
        } else if (state[num3] == a_NET_WM_WINDOW_TYPE_UTILITY) {
            SU_LOG_DEBUG2( "NET_WM_WINDOW_TYPE_UTILITY ");
            nwwt->utility = 1;
        } else if (state[num3] == a_NET_WM_WINDOW_TYPE_SPLASH) {
            SU_LOG_DEBUG2( "NET_WM_WINDOW_TYPE_SPLASH ");
            nwwt->splash = 1;
        } else if (state[num3] == a_NET_WM_WINDOW_TYPE_DIALOG) {
            su_log_debug( "NET_WM_WINDOW_TYPE_DIALOG ");
            nwwt->dialog = 1;
        } else if (state[num3] == a_NET_WM_WINDOW_TYPE_NORMAL) {
            SU_LOG_DEBUG2( "NET_WM_WINDOW_TYPE_NORMAL ");
            nwwt->normal = 1;
        } else {
            SU_LOG_DEBUG2( "... ");
        }
    }
    XFree(state);
    SU_LOG_DEBUG2( "}\n");
}

int wtl_x11_get_wm_state (Window win)
{
    unsigned long *data;
    int ret = 0;

    data = wtl_x11_get_xa_property (win, aWM_STATE, aWM_STATE, 0);
    if (data) {
        ret = data[0];
        XFree (data);
    }
    return ret;
}

typedef enum
{
    _MWM_DECOR_ALL      = 1 << 0, /*!< All decorations */
    _MWM_DECOR_BORDER   = 1 << 1, /*!< Show a border */
    _MWM_DECOR_HANDLE   = 1 << 2, /*!< Show a handle (bottom) */
    _MWM_DECOR_TITLE    = 1 << 3, /*!< Show a titlebar */
#if 0
    _MWM_DECOR_MENU     = 1 << 4, /*!< Show a menu */
#endif
    _MWM_DECOR_ICONIFY  = 1 << 5, /*!< Show an iconify button */
    _MWM_DECOR_MAXIMIZE = 1 << 6  /*!< Show a maximize button */
} MwmDecorations;

#define PROP_MOTIF_WM_HINTS_ELEMENTS 5
#define MWM_HINTS_DECORATIONS (1L << 1)
struct MwmHints {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
};

void
set_decorations (Window win, gboolean decorate)
{
    struct MwmHints hints = {0,};
    hints.flags = MWM_HINTS_DECORATIONS;
    hints.decorations = decorate ? _MWM_DECOR_ALL : (_MWM_DECOR_BORDER | _MWM_DECOR_HANDLE) ;

    /* Set Motif hints, most window managers handle these */
    XChangeProperty(wtl_x11_display(), win,
                    a_MOTIF_WM_HINTS, 
                    a_MOTIF_WM_HINTS, 32, PropModeReplace, 
                    (unsigned char *)&hints, PROP_MOTIF_WM_HINTS_ELEMENTS);

    Xclimsg(win, a_NET_WM_STATE,
                decorate ? 0 : 1,
                a_OB_WM_STATE_UNDECORATED,
                0, 0, 0);
}

int
get_mvm_decorations(Window win)
{
    gboolean result = -1;

    struct MwmHints * hints;
    int nitems = 0;

    hints = (struct MwmHints *) wtl_x11_get_xa_property(win, a_MOTIF_WM_HINTS, a_MOTIF_WM_HINTS, &nitems);

    if (!hints || nitems < PROP_MOTIF_WM_HINTS_ELEMENTS)
    {
        /* nothing */
    }
    else
    {
        if (hints->flags & MWM_HINTS_DECORATIONS)
        {
            result = (hints->decorations & (_MWM_DECOR_ALL | _MWM_DECOR_TITLE)) ? 1 : 0;
        }
        else
        {
            result = 1;
        }
    }

    if (hints)
        XFree(hints);

    return result;
}

gboolean get_decorations (Window win, NetWMState * nws)
{
    if (wtl_x11_check_net_supported(a_OB_WM_STATE_UNDECORATED))
    {
        NetWMState n;
        if (!nws)
        {
            nws = &n;
            wtl_x11_get_net_wm_state(win, nws);
        }
        return !nws->ob_undecorated;
    }
    else
    {
        return get_mvm_decorations(win) != 0;
    }
}


static Atom * _net_supported = NULL;
static int _net_supported_nitems = 0;

void wtl_x11_update_net_supported(void)
{
    if (_net_supported)
    {
        XFree(_net_supported);
        _net_supported = NULL;
        _net_supported_nitems = 0;
    }

    _net_supported = wtl_x11_get_xa_property(wtl_x11_root(), a_NET_SUPPORTED, XA_ATOM, &_net_supported_nitems);
}

gboolean wtl_x11_check_net_supported(Atom atom)
{
    if (_net_supported_nitems < 1 || !_net_supported_nitems)
        return FALSE;

    int i;
    for (i = 0; i < _net_supported_nitems; i++)
    {
        if (_net_supported[i] == atom)
            return TRUE;
    }

    return FALSE;
}

#include <X11/extensions/Xcomposite.h>

gboolean wtl_x11_is_composite_available(void)
{
    static int result = -1;

    if (result < 0)
    {
        int event_base, error_base, major, minor;
        if (!XCompositeQueryExtension(wtl_x11_display(), &event_base, &error_base))
        {
            result = FALSE;
        }
        else
        {
            major = 0, minor = 2;
            XCompositeQueryVersion(wtl_x11_display(), &major, &minor);
            if (! (major > 0 || minor >= 2))
            {
                result = FALSE;
            }
            else
            {
                result = TRUE;
            }
        }
    }

    return result;
}

/******************************************************************************/

/******************************************************************************/

static GdkPixbuf * get_net_wm_icon(Window task_win, int required_width, int required_height)
{
    GdkPixbuf * pixmap = NULL;
    int result;

    /* Important Notes:
     * According to freedesktop.org document:
     * http://standards.freedesktop.org/wm-spec/wm-spec-1.4.html#idm139915842350096
     * _NET_WM_ICON contains an array of 32-bit packed CARDINAL ARGB.
     * However, this is incorrect. Actually it's an array of long integers.
     * Toolkits like gtk+ use unsigned long here to store icons.
     * Besides, according to manpage of XGetWindowProperty, when returned format,
     * is 32, the property data will be stored as an array of longs
     * (which in a 64-bit application will be 64-bit values that are
     * padded in the upper 4 bytes).
     */

    /* Get the window property _NET_WM_ICON. */
    Atom type = None;
    int format;
    gulong nitems;
    gulong bytes_after;
    gulong * data = NULL;
    result = XGetWindowProperty(
        wtl_x11_display(),
        task_win,
        a_NET_WM_ICON,
        0, G_MAXLONG,
        False, XA_CARDINAL,
        &type, &format, &nitems, &bytes_after, (void *) &data);

    /* Inspect the result to see if it is usable.  If not, and we got data, free it. */
    if ((result != Success) || (type != XA_CARDINAL) || (nitems <= 0))
    {
        if (data != NULL)
            XFree(data);
        return NULL;
    }


    /*
        Choose the best icon size. In order:
        1. The exact required size.
        2. The maximum size in the range of 2x...4x.
        3. The maximum size.
    */
    gulong * pdata = data;
    gulong * pdata_end = data + nitems;
    gulong * max_icon = NULL; gulong max_w = 0; gulong max_h = 0;
    gulong * best_icon = NULL; gulong best_w = 0; gulong best_h = 0;
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

        /* The desired size is the same as icon size. */
        if ((required_width == w) && (required_height == h))
        {
            best_icon = pdata;
            best_w = w;
            best_h = h;
            break;
        }

        /* If the icon is the largest so far, capture it. */
        if ((w > max_w) && (h > max_h))
        {
            max_icon = pdata;
            max_w = w;
            max_h = h;
        }

        if ((w >= required_width * 2) && (w <= required_width * 4) && (h >= required_height * 2) && (h <= required_height * 4))
        {
            if ((w > best_w) && (h > best_h))
            {
                best_icon = pdata;
                best_w = w;
                best_h = h;
            }
        }

        pdata += size;
    }

    if (!best_icon)
    {
        best_icon = max_icon;
        best_w = max_w;
        best_h = max_h;
    }

    /* Сonvert the icon to GdkPixbuf. */
    if (best_icon != NULL)
    {
        /* Allocate enough space for the pixel data. */
        gulong len = best_w * best_h;
        guchar * pixdata = g_new(guchar, len * 4);

        /* Loop to convert the pixel data. */
        guchar * p = pixdata;
        int i;
        for (i = 0; i < len; p += 4, i += 1)
        {
            guint argb = best_icon[i];
            p[0] = (argb >> 16) & 0xff;
            p[1] = (argb >>  8) & 0xff;
            p[2] = (argb >>  0) & 0xff;
            p[3] = (argb >> 24) & 0xff;
        }

        /* Initialize a pixmap with the pixel data. */
        if (pixdata)
            pixmap = gdk_pixbuf_new_from_data(
                pixdata,
                GDK_COLORSPACE_RGB,
                TRUE, 8, /* has_alpha, bits_per_sample */
                best_w, best_h, best_w * 4,
                (GdkPixbufDestroyNotify) g_free,
                NULL);
    }

    /*g_print("required_width %d, required_height %d\nbest_w %lu, best_h %lu\nmax_w %lu, max_h %lu\n",
        required_width, required_height, best_w, best_h, max_w, max_h);*/

    /* Free the X property data. */
    XFree(data);

    return pixmap;
}

static GdkPixbuf * get_icon_from_pixmap_mask(Pixmap xpixmap, Pixmap xmask)
{
    GdkPixbuf * pixmap = NULL;
    int result;

    /* get pixmap geometry.*/
    unsigned int w, h;
    {
        Window unused_win;
        int unused;
        unsigned int unused_2;
        result = XGetGeometry(
            wtl_x11_display(), xpixmap,
            &unused_win, &unused, &unused, &w, &h, &unused_2, &unused_2) ? Success : -1;
    }

    /* convert it to a GDK pixbuf. */
    if (result == Success) 
    {
        pixmap = su_gdk_pixbuf_get_from_pixmap(xpixmap, w, h);
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
            wtl_x11_display(), xmask,
            &unused_win, &unused, &unused, &w, &h, &unused_2, &unused_2))
        {
            /* Convert the X mask to a GDK pixmap. */
            GdkPixbuf * mask = su_gdk_pixbuf_get_from_pixmap(xmask, w, h);
            if (mask != NULL)
            {
                /* Apply the mask. */
                GdkPixbuf * masked_pixmap = su_gdk_pixbuf_apply_mask(pixmap, mask);
                g_object_unref(G_OBJECT(pixmap));
                g_object_unref(G_OBJECT(mask));
                pixmap = masked_pixmap;
            }
        }
    }

    return pixmap;
}

static GdkPixbuf * get_icon_from_wm_hints(Window task_win)
{
    GdkPixbuf * pixmap = NULL;
    int result;

    XWMHints * hints = XGetWMHints(wtl_x11_display(), task_win);
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
        }
        else
            result = -1;
    }

    if (result == Success)
    {
        pixmap = get_icon_from_pixmap_mask(xpixmap, xmask);
    }

    return pixmap;
}

static GdkPixbuf * get_icon_from_kwm_win_icon(Window task_win)
{
    GdkPixbuf * pixmap = NULL;
    int result;

    Pixmap xpixmap = None;
    Pixmap xmask = None;

    Atom type = None;
    int format;
    gulong nitems;
    gulong bytes_after;
    Pixmap *icons = NULL;
    Atom kwin_win_icon_atom = gdk_x11_get_xatom_by_name("KWM_WIN_ICON");
    result = XGetWindowProperty(
        wtl_x11_display(),
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
        }
        else
            result = -1;
    }

    if (result == Success)
    {
        pixmap = get_icon_from_pixmap_mask(xpixmap, xmask);
    }

    return pixmap;

}

/* Get an icon from the window manager for a task, and scale it to a specified size. */
GdkPixbuf * wtl_x11_get_wm_icon(Window task_win, int required_width, int required_height, Atom source, Atom * current_source)
{
    /* The result. */
    GdkPixbuf * pixmap = NULL;
    Atom possible_source = None;

    Atom kwin_win_icon_atom = gdk_x11_get_xatom_by_name("KWM_WIN_ICON");

    /* First, try to load icon from the `source` source. */

    Atom preferable_source = source;

    again:

    if (!pixmap && preferable_source == a_NET_WM_ICON)
    {
        pixmap = get_net_wm_icon(task_win, required_width, required_height);
        if (pixmap)
            possible_source = a_NET_WM_ICON;
    }

    if (!pixmap && preferable_source == XA_WM_HINTS)
    {
        pixmap = get_icon_from_wm_hints(task_win);
        if (pixmap)
            possible_source = XA_WM_HINTS;
    }

    if (!pixmap && preferable_source == kwin_win_icon_atom)
    {
        pixmap = get_icon_from_kwm_win_icon(task_win);
        if (pixmap)
            possible_source = kwin_win_icon_atom;
    }

    /* Second, try to load icon from the source that has succeed previous time. */

    if (!pixmap && *current_source && preferable_source != *current_source)
    {
        preferable_source = *current_source;
        goto again;
    }

    /* Third, try each source. */

    if (!pixmap)
    {
        pixmap = get_net_wm_icon(task_win, required_width, required_height);
        if (pixmap)
            possible_source = a_NET_WM_ICON;
    }

    if (!pixmap)
    {
        pixmap = get_icon_from_wm_hints(task_win);
        if (pixmap)
            possible_source = XA_WM_HINTS;
    }

    if (!pixmap)
    {
        pixmap = get_icon_from_kwm_win_icon(task_win);
        if (pixmap)
            possible_source = kwin_win_icon_atom;
    }

    if (pixmap)
        *current_source = possible_source;

    return pixmap;
}

void wtl_x11_set_wmhints_no_input(Window w)
{
    XWMHints wmhints;
    wmhints.flags = InputHint;
    wmhints.input = 0;
    XSetWMHints (wtl_x11_display(), w, &wmhints);
}

void wtl_x11_set_win_hints_skip_focus(Window w)
{
    #define WIN_HINTS_SKIP_FOCUS      (1<<0)    /* "alt-tab" skips this win */
    guint32 val = WIN_HINTS_SKIP_FOCUS;
    XChangeProperty(wtl_x11_display(), w,
          XInternAtom(wtl_x11_display(), "_WIN_HINTS", False), XA_CARDINAL, 32,
          PropModeReplace, (unsigned char *) &val, 1);
}

gboolean wtl_x11_get_net_showing_desktop_supported(void)
{
    return gdk_x11_screen_supports_net_wm_hint(gdk_screen_get_default(),
        gdk_atom_intern("_NET_SHOWING_DESKTOP", FALSE));
}

gboolean wtl_x11_get_net_showing_desktop(void)
{
    gboolean result = FALSE;
    guint32 * data = wtl_x11_get_xa_property (wtl_x11_root(), a_NET_SHOWING_DESKTOP, XA_CARDINAL, 0);
    if (data)
    {
        result = *data;
        XFree (data);
    }
    return result;
}

void wtl_x11_set_net_showing_desktop(gboolean value)
{
    Xclimsg(wtl_x11_root(), a_NET_SHOWING_DESKTOP, value, 0, 0, 0, 0);
}

/****************************************************************************/

gboolean wtl_x11_is_my_own_window(Window window)
{
    return !!gdk_window_lookup(window);
}
