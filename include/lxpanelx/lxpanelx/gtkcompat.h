/**
 * Copyright (c) 2011 Vadim Ushakov
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

#ifndef GTKCOMPAT_H
#define GTKCOMPAT_H

#include <gmodule.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>


#if !GTK_CHECK_VERSION(2,14,0)

static inline void
hsv_to_rgb (gdouble *h,
	    gdouble *s,
	    gdouble *v)
{
  gdouble hue, saturation, value;
  gdouble f, p, q, t;

  if (*s == 0.0)
    {
      *h = *v;
      *s = *v;
      *v = *v; /* heh */
    }
  else
    {
      hue = *h * 6.0;
      saturation = *s;
      value = *v;

      if (hue == 6.0)
	hue = 0.0;

      f = hue - (int) hue;
      p = value * (1.0 - saturation);
      q = value * (1.0 - saturation * f);
      t = value * (1.0 - saturation * (1.0 - f));

      switch ((int) hue)
	{
	case 0:
	  *h = value;
	  *s = t;
	  *v = p;
	  break;

	case 1:
	  *h = q;
	  *s = value;
	  *v = p;
	  break;

	case 2:
	  *h = p;
	  *s = value;
	  *v = t;
	  break;

	case 3:
	  *h = p;
	  *s = q;
	  *v = value;
	  break;

	case 4:
	  *h = t;
	  *s = p;
	  *v = value;
	  break;

	case 5:
	  *h = value;
	  *s = p;
	  *v = q;
	  break;

	default:
	  g_assert_not_reached ();
	}
    }
}

static inline void
rgb_to_hsv (gdouble *r,
	    gdouble *g,
	    gdouble *b)
{
  gdouble red, green, blue;
  gdouble h, s, v;
  gdouble min, max;
  gdouble delta;

  red = *r;
  green = *g;
  blue = *b;

  h = 0.0;

  if (red > green)
    {
      if (red > blue)
	max = red;
      else
	max = blue;

      if (green < blue)
	min = green;
      else
	min = blue;
    }
  else
    {
      if (green > blue)
	max = green;
      else
	max = blue;

      if (red < blue)
	min = red;
      else
	min = blue;
    }

  v = max;

  if (max != 0.0)
    s = (max - min) / max;
  else
    s = 0.0;

  if (s == 0.0)
    h = 0.0;
  else
    {
      delta = max - min;

      if (red == max)
	h = (green - blue) / delta;
      else if (green == max)
	h = 2 + (blue - red) / delta;
      else if (blue == max)
	h = 4 + (red - green) / delta;

      h /= 6.0;

      if (h < 0.0)
	h += 1.0;
      else if (h > 1.0)
	h -= 1.0;
    }

  *r = h;
  *g = s;
  *b = v;
}

static inline void
gtk_hsv_to_rgb (gdouble  h,
		gdouble  s,
		gdouble  v,
		gdouble *r,
		gdouble *g,
		gdouble *b)
{
  g_return_if_fail (h >= 0.0 && h <= 1.0);
  g_return_if_fail (s >= 0.0 && s <= 1.0);
  g_return_if_fail (v >= 0.0 && v <= 1.0);

  hsv_to_rgb (&h, &s, &v);

  if (r)
    *r = h;

  if (g)
    *g = s;

  if (b)
    *b = v;
}

static inline void
gtk_rgb_to_hsv (gdouble  r,
		gdouble  g,
		gdouble  b,
		gdouble *h,
		gdouble *s,
		gdouble *v)
{
  g_return_if_fail (r >= 0.0 && r <= 1.0);
  g_return_if_fail (g >= 0.0 && g <= 1.0);
  g_return_if_fail (b >= 0.0 && b <= 1.0);

  rgb_to_hsv (&r, &g, &b);

  if (h)
    *h = r;

  if (s)
    *s = g;

  if (v)
    *v = b;
}

static inline
GdkWindow * gtk_widget_get_window(GtkWidget *widget)
{
    return widget->window;
}

#endif // !GTK_CHECK_VERSION(2,14,0)


#if !GTK_CHECK_VERSION(2,18,0)

static inline
void gtk_widget_set_visible(GtkWidget *widget, gboolean visible)
{
    (visible ? gtk_widget_show : gtk_widget_hide)(widget);
}

static inline
gboolean gtk_widget_get_visible (GtkWidget *widget)
{
    return GTK_WIDGET_VISIBLE(widget) ? TRUE : FALSE;
}

static inline
void gtk_widget_set_has_window(GtkWidget *widget, gboolean has_window)
{
    if (!has_window) {
       GTK_WIDGET_SET_FLAGS (widget, GTK_NO_WINDOW);
    } else {
       GTK_WIDGET_UNSET_FLAGS (widget, GTK_NO_WINDOW);
    }
}

static inline
gboolean gtk_widget_get_has_window (GtkWidget *widget)
{
    return GTK_WIDGET_NO_WINDOW(widget) ? FALSE : TRUE;
}

static inline
void gtk_widget_set_can_focus(GtkWidget *widget, gboolean can_focus)
{
    if (can_focus) {
       GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
    } else {
       GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS);
    }
}

static inline
void gtk_widget_set_can_default(GtkWidget *widget, gboolean can_default)
{
    if (can_default) {
       GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
    } else {
       GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_DEFAULT);
    }
}

static inline
GtkStateType gtk_widget_get_state(GtkWidget *widget)
{
    return GTK_WIDGET_STATE(widget);
}

#endif // !GTK_CHECK_VERSION(2,18,0)


#if !GTK_CHECK_VERSION(2,20,0)

static inline
void gtk_widget_set_realized(GtkWidget *widget, gboolean realized)
{
    if (realized) {
       GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    } else {
       GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED);
    }
}

static inline
gboolean gtk_widget_get_realized(GtkWidget *widget)
{
    return GTK_WIDGET_REALIZED(widget);
}

static inline
gboolean gtk_widget_get_mapped(GtkWidget *widget)
{
    return GTK_WIDGET_MAPPED(widget);
}

#endif // !GTK_CHECK_VERSION(2,20,0)


#if !GTK_CHECK_VERSION(2,22,0)

static  inline
gint gdk_visual_get_depth (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), 0);

  return visual->depth;
}

static inline
void gdk_visual_get_red_pixel_details (GdkVisual *visual, guint32 *mask, gint *shift, gint *precision)
{
  g_return_if_fail (GDK_IS_VISUAL (visual));

  if (mask)
    *mask = visual->red_mask;

  if (shift)
    *shift = visual->red_shift;

  if (precision)
    *precision = visual->red_prec;
}

static inline
void gdk_visual_get_green_pixel_details (GdkVisual *visual, guint32 *mask, gint *shift, gint *precision)
{
  g_return_if_fail (GDK_IS_VISUAL (visual));

  if (mask)
    *mask = visual->green_mask;

  if (shift)
    *shift = visual->green_shift;

  if (precision)
    *precision = visual->green_prec;
}

static inline
void gdk_visual_get_blue_pixel_details (GdkVisual *visual, guint32 *mask, gint *shift, gint *precision)
{
  g_return_if_fail (GDK_IS_VISUAL (visual));

  if (mask)
    *mask = visual->blue_mask;

  if (shift)
    *shift = visual->blue_shift;

  if (precision)
    *precision = visual->blue_prec;
}

GdkWindow * gtk_button_get_event_window(GtkButton *button)
{
    return button->event_window;
}

#endif // !GTK_CHECK_VERSION(2,22,0)


#if !GTK_CHECK_VERSION(2,24,0)

#include <X11/X.h>

static inline
GdkWindow * gdk_x11_window_foreign_new_for_display(GdkDisplay *display, Window window)
{
    return gdk_window_foreign_new_for_display(display, window);
}

static inline
GdkWindow * gdk_x11_window_lookup_for_display(GdkDisplay *display, Window window)
{
    return gdk_window_lookup_for_display(display, window);
}

static inline
void gdk_pixmap_get_size(GdkPixmap *pixmap, gint *width, gint *height)
{
    gdk_drawable_get_size(pixmap, width, height);
}

#endif // !GTK_CHECK_VERSION(2,24,0)


#endif
