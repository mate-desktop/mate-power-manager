/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * On-screen-display (OSD) window for mate-settings-daemon's plugins
 *
 * Copyright (C) 2006-2007 William Jon McCann <mccann@jhu.edu> 
 * Copyright (C) 2009 Novell, Inc
 *
 * Authors:
 *   William Jon McCann <mccann@jhu.edu>
 *   Federico Mena-Quintero <federico@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "msd-osd-window.h"

#define DIALOG_TIMEOUT 2000     /* dialog timeout in ms */
#define DIALOG_FADE_TIMEOUT 1500 /* timeout before fade starts */
#define FADE_TIMEOUT 10        /* timeout in ms between each frame of the fade */

#define BG_ALPHA 0.75

#define MSD_OSD_WINDOW_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MSD_TYPE_OSD_WINDOW, MsdOsdWindowPrivate))

struct MsdOsdWindowPrivate
{
        guint                    is_composited : 1;
        guint                    hide_timeout_id;
        guint                    fade_timeout_id;
        double                   fade_out_alpha;
};

enum {
        DRAW_WHEN_COMPOSITED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (MsdOsdWindow, msd_osd_window, GTK_TYPE_WINDOW)

static gboolean
fade_timeout (MsdOsdWindow *window)
{
        if (window->priv->fade_out_alpha <= 0.0) {
                gtk_widget_hide (GTK_WIDGET (window));

                /* Reset it for the next time */
                window->priv->fade_out_alpha = 1.0;
                window->priv->fade_timeout_id = 0;

                return FALSE;
        } else {
                GdkRectangle rect;
                GtkWidget *win = GTK_WIDGET (window);
                GtkAllocation allocation;

                window->priv->fade_out_alpha -= 0.10;

                rect.x = 0;
                rect.y = 0;
                gtk_widget_get_allocation (win, &allocation);
                rect.width = allocation.width;
                rect.height = allocation.height;

                gtk_widget_realize (win);
                gdk_window_invalidate_rect (gtk_widget_get_window (win), &rect, FALSE);
        }

        return TRUE;
}

static gboolean
hide_timeout (MsdOsdWindow *window)
{
        if (window->priv->is_composited) {
                window->priv->hide_timeout_id = 0;
                window->priv->fade_timeout_id = g_timeout_add (FADE_TIMEOUT,
                                                               (GSourceFunc) fade_timeout,
                                                               window);
        } else {
                gtk_widget_hide (GTK_WIDGET (window));
        }

        return FALSE;
}

static void
remove_hide_timeout (MsdOsdWindow *window)
{
        if (window->priv->hide_timeout_id != 0) {
                g_source_remove (window->priv->hide_timeout_id);
                window->priv->hide_timeout_id = 0;
        }

        if (window->priv->fade_timeout_id != 0) {
                g_source_remove (window->priv->fade_timeout_id);
                window->priv->fade_timeout_id = 0;
                window->priv->fade_out_alpha = 1.0;
        }
}

static void
add_hide_timeout (MsdOsdWindow *window)
{
        int timeout;

        if (window->priv->is_composited) {
                timeout = DIALOG_FADE_TIMEOUT;
        } else {
                timeout = DIALOG_TIMEOUT;
        }
        window->priv->hide_timeout_id = g_timeout_add (timeout,
                                                       (GSourceFunc) hide_timeout,
                                                       window);
}

/* This is our draw-event handler when the window is in a compositing manager.
 * We draw everything by hand, using Cairo, so that we can have a nice
 * transparent/rounded look.
 */
static void
draw_when_composited (GtkWidget *widget, cairo_t *orig_cr)
{
        MsdOsdWindow    *window;
        cairo_t         *cr;
        cairo_surface_t *surface;
        int              width;
        int              height;
        GtkStyleContext *context;

        window = MSD_OSD_WINDOW (widget);

        context = gtk_widget_get_style_context (widget);
        cairo_set_operator (orig_cr, CAIRO_OPERATOR_SOURCE);
        gtk_window_get_size (GTK_WINDOW (widget), &width, &height);

        surface = cairo_surface_create_similar (cairo_get_target (orig_cr),
                                                CAIRO_CONTENT_COLOR_ALPHA,
                                                width,
                                                height);

        if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS) {
                goto done;
        }

        cr = cairo_create (surface);
        if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
                goto done;
        }

        gtk_render_background (context, cr, 0, 0, width, height);
        gtk_render_frame (context, cr, 0, 0, width, height);

        g_signal_emit (window, signals[DRAW_WHEN_COMPOSITED], 0, cr);

        cairo_destroy (cr);

        /* Make sure we have a transparent background */
        cairo_rectangle (orig_cr, 0, 0, width, height);
        cairo_set_source_rgba (orig_cr, 0.0, 0.0, 0.0, 0.0);
        cairo_fill (orig_cr);

        cairo_set_source_surface (orig_cr, surface, 0, 0);
        cairo_paint_with_alpha (orig_cr, window->priv->fade_out_alpha);

done:
        if (surface != NULL) {
                cairo_surface_destroy (surface);
        }
}

/* This is our expose/draw-event handler when the window is *not* in a compositing manager.
 * We just draw a rectangular frame by hand.  We do this with hardcoded drawing code,
 * instead of GtkFrame, to avoid changing the window's internal widget hierarchy:  in
 * either case (composited or non-composited), callers can assume that this works
 * identically to a GtkWindow without any intermediate widgetry.
 */
static void
draw_when_not_composited (GtkWidget *widget, cairo_t *cr)
{
        GtkStyleContext *context;
        int width;
        int height;

        gtk_window_get_size (GTK_WINDOW (widget), &width, &height);
        context = gtk_widget_get_style_context (widget);

        gtk_style_context_set_state (context, GTK_STATE_FLAG_ACTIVE);
        gtk_style_context_add_class(context,"msd-osd-window-solid");
        gtk_render_frame (context,
                          cr,
                          0,
                          0,
                          width,
                          height);
}

static gboolean
msd_osd_window_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
	MsdOsdWindow *window;
	GtkWidget *child;

	window = MSD_OSD_WINDOW (widget);

	if (window->priv->is_composited)
		draw_when_composited (widget, cr);
	else
		draw_when_not_composited (widget, cr);

	child = gtk_bin_get_child (GTK_BIN (window));
	if (child)
		gtk_container_propagate_draw (GTK_CONTAINER (window), child, cr);

	return FALSE;
}

static void
msd_osd_window_real_show (GtkWidget *widget)
{
        MsdOsdWindow *window;

        if (GTK_WIDGET_CLASS (msd_osd_window_parent_class)->show) {
                GTK_WIDGET_CLASS (msd_osd_window_parent_class)->show (widget);
        }

        window = MSD_OSD_WINDOW (widget);
        remove_hide_timeout (window);
        add_hide_timeout (window);
}

static void
msd_osd_window_real_hide (GtkWidget *widget)
{
        MsdOsdWindow *window;

        if (GTK_WIDGET_CLASS (msd_osd_window_parent_class)->hide) {
                GTK_WIDGET_CLASS (msd_osd_window_parent_class)->hide (widget);
        }

        window = MSD_OSD_WINDOW (widget);
        remove_hide_timeout (window);
}

static void
msd_osd_window_real_realize (GtkWidget *widget)
{
        GdkScreen *screen;
        GdkVisual *visual;
        cairo_region_t *region;

        screen = gtk_widget_get_screen (widget);
        visual = gdk_screen_get_rgba_visual (screen);

        if (visual == NULL) {
                visual = gdk_screen_get_system_visual (screen);
        }

        gtk_widget_set_visual (widget, visual);

        if (GTK_WIDGET_CLASS (msd_osd_window_parent_class)->realize) {
                GTK_WIDGET_CLASS (msd_osd_window_parent_class)->realize (widget);
        }

        /* make the whole window ignore events */
        region = cairo_region_create ();
        gtk_widget_input_shape_combine_region (widget, region);
        cairo_region_destroy (region);
}

static void
msd_osd_window_style_updated (GtkWidget *widget)
{
        GtkStyleContext *context;
        GtkBorder padding;

        GTK_WIDGET_CLASS (msd_osd_window_parent_class)->style_updated (widget);

        /* We set our border width to 12 (per the MATE standard), plus the
         * padding of the frame that we draw in our expose handler.  This will
         * make our child be 12 pixels away from the frame.
         */

        context = gtk_widget_get_style_context (widget);
        gtk_style_context_get_padding (context, GTK_STATE_FLAG_NORMAL, &padding);
        gtk_container_set_border_width (GTK_CONTAINER (widget), 12 + MAX (padding.left, padding.top));
}

static void
msd_osd_window_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
        GtkStyleContext *context;
        GtkBorder padding;

        GTK_WIDGET_CLASS (msd_osd_window_parent_class)->get_preferred_width (widget, minimum, natural);

        /* See the comment in msd_osd_window_style_updated() for why we add the padding here */

        context = gtk_widget_get_style_context (widget);
        gtk_style_context_get_padding (context, GTK_STATE_FLAG_NORMAL, &padding);

        *minimum += padding.left;
        *natural += padding.left;
}

static void
msd_osd_window_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
        GtkStyleContext *context;
        GtkBorder padding;

        GTK_WIDGET_CLASS (msd_osd_window_parent_class)->get_preferred_height (widget, minimum, natural);

        /* See the comment in msd_osd_window_style_updated() for why we add the padding here */

        context = gtk_widget_get_style_context (widget);
        gtk_style_context_get_padding (context, GTK_STATE_FLAG_NORMAL, &padding);

        *minimum += padding.top;
        *natural += padding.top;
}

static GObject *
msd_osd_window_constructor (GType                  type,
                            guint                  n_construct_properties,
                            GObjectConstructParam *construct_params)
{
        GObject *object;

        object = G_OBJECT_CLASS (msd_osd_window_parent_class)->constructor (type, n_construct_properties, construct_params);

        g_object_set (object,
                      "type", GTK_WINDOW_POPUP,
                      "type-hint", GDK_WINDOW_TYPE_HINT_NOTIFICATION,
                      "skip-taskbar-hint", TRUE,
                      "skip-pager-hint", TRUE,
                      "focus-on-map", FALSE,
                      NULL);

        GtkWidget *widget = GTK_WIDGET (object);
        GtkStyleContext *style_context = gtk_widget_get_style_context (widget);
        gtk_style_context_add_class (style_context, "osd");

        return object;
}

static void
msd_osd_window_class_init (MsdOsdWindowClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gobject_class->constructor = msd_osd_window_constructor;

        widget_class->show = msd_osd_window_real_show;
        widget_class->hide = msd_osd_window_real_hide;
        widget_class->realize = msd_osd_window_real_realize;
        widget_class->style_updated = msd_osd_window_style_updated;
        widget_class->get_preferred_width = msd_osd_window_get_preferred_width;
        widget_class->get_preferred_height = msd_osd_window_get_preferred_height;
        widget_class->draw = msd_osd_window_draw;

        signals[DRAW_WHEN_COMPOSITED] = g_signal_new ("draw-when-composited",
                                                        G_TYPE_FROM_CLASS (gobject_class),
                                                        G_SIGNAL_RUN_FIRST,
                                                        G_STRUCT_OFFSET (MsdOsdWindowClass, draw_when_composited),
                                                        NULL, NULL,
                                                        g_cclosure_marshal_VOID__POINTER,
                                                        G_TYPE_NONE, 1,
                                                        G_TYPE_POINTER);

#if GTK_CHECK_VERSION (3, 20, 0)
        gtk_widget_class_set_css_name (widget_class, "MsdOsdWindow");
#endif
        g_type_class_add_private (klass, sizeof (MsdOsdWindowPrivate));
}

/**
 * msd_osd_window_is_composited:
 * @window: a #MsdOsdWindow
 *
 * Return value: whether the window was created on a composited screen.
 */
gboolean
msd_osd_window_is_composited (MsdOsdWindow *window)
{
        return window->priv->is_composited;
}

/**
 * msd_osd_window_is_valid:
 * @window: a #MsdOsdWindow
 *
 * Return value: TRUE if the @window's idea of being composited matches whether
 * its current screen is actually composited.
 */
gboolean
msd_osd_window_is_valid (MsdOsdWindow *window)
{
        GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (window));
        return gdk_screen_is_composited (screen) == window->priv->is_composited;
}

static void
msd_osd_window_init (MsdOsdWindow *window)
{
        GdkScreen *screen;

        window->priv = MSD_OSD_WINDOW_GET_PRIVATE (window);

        screen = gtk_widget_get_screen (GTK_WIDGET (window));

        window->priv->is_composited = gdk_screen_is_composited (screen);

        if (window->priv->is_composited) {
                gdouble scalew, scaleh, scale;
                gint sc_width, sc_height;
                gint size;

                gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
                gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);

                GtkStyleContext *style = gtk_widget_get_style_context (GTK_WIDGET (window));
                gtk_style_context_add_class (style, "window-frame");

                /* assume 130x130 on a 640x480 display and scale from there */

                gdk_window_get_geometry (gdk_screen_get_root_window (screen), NULL, NULL,
                                         &sc_width, &sc_height);

                scalew = sc_width / 640.0;
                scaleh = sc_height / 480.0;
                scale = MIN (scalew, scaleh);
                size = 130 * MAX (1, scale);

                gtk_window_set_default_size (GTK_WINDOW (window), size, size);

                window->priv->fade_out_alpha = 1.0;
        } else {
		gtk_container_set_border_width (GTK_CONTAINER (window), 12);
        }
}

GtkWidget *
msd_osd_window_new (void)
{
        return g_object_new (MSD_TYPE_OSD_WINDOW, NULL);
}

/**
 * msd_osd_window_update_and_hide:
 * @window: a #MsdOsdWindow
 *
 * Queues the @window for immediate drawing, and queues a timer to hide the window.
 */
void
msd_osd_window_update_and_hide (MsdOsdWindow *window)
{
        remove_hide_timeout (window);
        add_hide_timeout (window);

        if (window->priv->is_composited) {
                gtk_widget_queue_draw (GTK_WIDGET (window));
        }
}
