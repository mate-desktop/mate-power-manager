/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2009 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012-2021 MATE Developers
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gdk/gdk.h>

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#include <gdk/gdkx.h>
#endif /* HAVE_X11 */

#ifdef HAVE_WAYLAND
#include <gdk/gdkwayland.h>
#include <wayland-client.h>
#include "ext-idle-notify-v1-client.h"
#endif /* HAVE_WAYLAND */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "egg-idletime.h"

static void     egg_idletime_finalize   (GObject       *object);
gint64          egg_idletime_get_time   (EggIdletime   *idletime);

struct EggIdletimePrivate
{
	gboolean		 reset_set;
	GPtrArray		*array;
#ifdef HAVE_X11
	gint			 sync_event;
	XSyncCounter		 idle_counter;
	Display			*dpy;
#endif
#ifdef HAVE_WAYLAND
	struct ext_idle_notifier_v1 *idle_notifier;
	struct wl_seat		 *seat;
	GPtrArray		*wayland_notifications; /* of EggIdletimeWaylandNotification* */
	gboolean		 wayland_is_idle;
	GTimer			*idle_timer;
#endif
};

#ifdef HAVE_X11
typedef struct
{
	guint			 id;
	XSyncValue		 timeout;
	XSyncAlarm		 xalarm;
	EggIdletime		*idletime;
} EggIdletimeAlarm;
#endif

#ifdef HAVE_WAYLAND
typedef struct
{
	guint			 id;
	guint			 timeout;
	struct ext_idle_notification_v1 *notification;
} EggIdletimeWaylandNotification;
#endif

enum {
	SIGNAL_ALARM_EXPIRED,
	SIGNAL_RESET,
	LAST_SIGNAL
};

#ifdef HAVE_X11
typedef enum {
	EGG_IDLETIME_ALARM_TYPE_POSITIVE,
	EGG_IDLETIME_ALARM_TYPE_NEGATIVE,
	EGG_IDLETIME_ALARM_TYPE_DISABLED
} EggIdletimeAlarmType;
#endif

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer egg_idletime_object = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (EggIdletime, egg_idletime, G_TYPE_OBJECT)

#ifdef HAVE_X11

/**
 * egg_idletime_xsyncvalue_to_int64:
 */
static gint64
egg_idletime_xsyncvalue_to_int64 (XSyncValue value)
{
	return ((guint64) XSyncValueHigh32 (value)) << 32 | (guint64) XSyncValueLow32 (value);
}

/**
 * egg_idletime_xsync_alarm_set:
 */
static void
egg_idletime_xsync_alarm_set (EggIdletime *idletime, EggIdletimeAlarm *alarm, EggIdletimeAlarmType alarm_type)
{
	XSyncAlarmAttributes attr;
	XSyncValue delta;
	unsigned int flags;
	XSyncTestType test;

	/* just remove it */
	if (alarm_type == EGG_IDLETIME_ALARM_TYPE_DISABLED) {
		if (alarm->xalarm) {
			XSyncDestroyAlarm (idletime->priv->dpy, alarm->xalarm);
			alarm->xalarm = None;
		}
		return;
	}

	/* which way do we do the test? */
	if (alarm_type == EGG_IDLETIME_ALARM_TYPE_POSITIVE)
		test = XSyncPositiveTransition;
	else
		test = XSyncNegativeTransition;

	XSyncIntToValue (&delta, 0);

	attr.trigger.counter = idletime->priv->idle_counter;
	attr.trigger.value_type = XSyncAbsolute;
	attr.trigger.test_type = test;
	attr.trigger.wait_value = alarm->timeout;
	attr.delta = delta;

	flags = XSyncCACounter | XSyncCAValueType | XSyncCATestType | XSyncCAValue | XSyncCADelta;

	if (alarm->xalarm)
		XSyncChangeAlarm (idletime->priv->dpy, alarm->xalarm, flags, &attr);
	else
		alarm->xalarm = XSyncCreateAlarm (idletime->priv->dpy, flags, &attr);
}

/**
 * egg_idletime_alarm_find_id:
 */
static EggIdletimeAlarm *
egg_idletime_alarm_find_id (EggIdletime *idletime, guint id)
{
	guint i;
	EggIdletimeAlarm *alarm;
	for (i=0; i<idletime->priv->array->len; i++) {
		alarm = g_ptr_array_index (idletime->priv->array, i);
		if (alarm->id == id)
			return alarm;
	}
	return NULL;
}

/**
 * egg_idletime_set_reset_alarm:
 */
static void
egg_idletime_set_reset_alarm (EggIdletime *idletime, XSyncAlarmNotifyEvent *alarm_event)
{
	EggIdletimeAlarm *alarm;
	int overflow;
	XSyncValue add;
	gint64 current, reset_threshold;

	alarm = egg_idletime_alarm_find_id (idletime, 0);

	if (!idletime->priv->reset_set) {
		/* don't match on the current value because
		 * XSyncNegativeComparison means less or equal. */
		XSyncIntToValue (&add, -1);
		XSyncValueAdd (&alarm->timeout, alarm_event->counter_value, add, &overflow);

		/* set the reset alarm to fire the next time
		 * idletime->priv->idle_counter < the current counter value */
		egg_idletime_xsync_alarm_set (idletime, alarm, EGG_IDLETIME_ALARM_TYPE_NEGATIVE);

		/* don't try to set this again if multiple timers are going off in sequence */
		idletime->priv->reset_set = TRUE;

		current = egg_idletime_get_time (idletime);
		reset_threshold = egg_idletime_xsyncvalue_to_int64 (alarm->timeout);
		if (current < reset_threshold) {
			/* We've missed the alarm already */
			egg_idletime_alarm_reset_all (idletime);
		}
	}
}

/**
 * egg_idletime_alarm_find_event:
 */
static EggIdletimeAlarm *
egg_idletime_alarm_find_event (EggIdletime *idletime, XSyncAlarmNotifyEvent *alarm_event)
{
	guint i;
	EggIdletimeAlarm *alarm;
	for (i=0; i<idletime->priv->array->len; i++) {
		alarm = g_ptr_array_index (idletime->priv->array, i);
		if (alarm_event->alarm == alarm->xalarm)
			return alarm;
	}
	return NULL;
}

/**
 * egg_idletime_event_filter_cb:
 */
static GdkFilterReturn
egg_idletime_event_filter_cb (GdkXEvent *gdkxevent, GdkEvent *event, gpointer data)
{
	EggIdletimeAlarm *alarm;
	XEvent *xevent = (XEvent *) gdkxevent;
	EggIdletime *idletime = (EggIdletime *) data;
	XSyncAlarmNotifyEvent *alarm_event;

	/* no point continuing */
	if (xevent->type != idletime->priv->sync_event + XSyncAlarmNotify)
		return GDK_FILTER_CONTINUE;

	alarm_event = (XSyncAlarmNotifyEvent *) xevent;

	/* did we match one of our alarms? */
	alarm = egg_idletime_alarm_find_event (idletime, alarm_event);
	if (alarm == NULL)
		return GDK_FILTER_CONTINUE;

	/* are we the reset alarm? */
	if (alarm->id == 0) {
		egg_idletime_alarm_reset_all (idletime);
		goto out;
	}

	/* emit */
	g_signal_emit (alarm->idletime, signals [SIGNAL_ALARM_EXPIRED], 0, alarm->id);

	/* we need the first alarm to go off to set the reset alarm */
	egg_idletime_set_reset_alarm (idletime, alarm_event);
out:
	/* don't propagate */
	return GDK_FILTER_REMOVE;
}

/**
 * egg_idletime_alarm_new:
 */
static EggIdletimeAlarm *
egg_idletime_alarm_new (EggIdletime *idletime, guint id)
{
	EggIdletimeAlarm *alarm;

	/* create a new alarm */
	alarm = g_new0 (EggIdletimeAlarm, 1);

	/* set the default values */
	alarm->id = id;
	alarm->xalarm = None;
	alarm->idletime = g_object_ref (idletime);

	return alarm;
}

/**
 * egg_idletime_alarm_free:
 */
static gboolean
egg_idletime_alarm_free (EggIdletime *idletime, EggIdletimeAlarm *alarm)
{
	g_return_val_if_fail (EGG_IS_IDLETIME (idletime), FALSE);
	g_return_val_if_fail (alarm != NULL, FALSE);

	if (alarm->xalarm)
		XSyncDestroyAlarm (idletime->priv->dpy, alarm->xalarm);
	g_object_unref (alarm->idletime);
	g_ptr_array_remove (idletime->priv->array, alarm);
	g_free (alarm);
	return TRUE;
}

#endif /* HAVE_X11 */

#ifdef HAVE_WAYLAND

/**
 * egg_idletime_alarm_wayland_find_id:
 */
static EggIdletimeWaylandNotification *
egg_idletime_alarm_wayland_find_id (EggIdletime *idletime, guint id)
{
	guint i;
	EggIdletimeWaylandNotification *wn;
	for (i = 0; i < idletime->priv->wayland_notifications->len; i++) {
		wn = g_ptr_array_index (idletime->priv->wayland_notifications, i);
		if (wn->id == id)
			return wn;
	}
	return NULL;
}

/**
 * egg_idletime_wayland_notification_idled_cb:
 */
static void
egg_idletime_wayland_notification_idled_cb (void *data,
					    struct ext_idle_notification_v1 *notification)
{
	EggIdletime *idletime = EGG_IDLETIME (data);
	EggIdletimeWaylandNotification *wn;
	guint i;

	for (i = 0; i < idletime->priv->wayland_notifications->len; i++) {
		wn = g_ptr_array_index (idletime->priv->wayland_notifications, i);
		if (wn->notification == notification) {
			g_signal_emit (idletime, signals [SIGNAL_ALARM_EXPIRED], 0, wn->id);
			break;
		}
	}
	idletime->priv->wayland_is_idle = TRUE;
	if (idletime->priv->idle_timer)
		g_timer_start (idletime->priv->idle_timer);
}

/**
 * egg_idletime_wayland_notification_resumed_cb:
 */
static void
egg_idletime_wayland_notification_resumed_cb (void *data,
					      struct ext_idle_notification_v1 *notification)
{
	EggIdletime *idletime = EGG_IDLETIME (data);

	if (idletime->priv->wayland_is_idle) {
		idletime->priv->wayland_is_idle = FALSE;
		g_signal_emit (idletime, signals [SIGNAL_RESET], 0);
	}
}

static const struct ext_idle_notification_v1_listener
egg_idletime_wayland_notification_listener = {
	.idled = egg_idletime_wayland_notification_idled_cb,
	.resumed = egg_idletime_wayland_notification_resumed_cb,
};

#endif /* HAVE_WAYLAND */

/**
 * egg_idletime_class_init:
 **/
static void
egg_idletime_class_init (EggIdletimeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = egg_idletime_finalize;

	signals [SIGNAL_ALARM_EXPIRED] =
		g_signal_new ("alarm-expired",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EggIdletimeClass, alarm_expired),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [SIGNAL_RESET] =
		g_signal_new ("reset",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EggIdletimeClass, reset),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/**
 * egg_idletime_get_time:
 * Return value: current idle time in milliseconds
 **/
gint64
egg_idletime_get_time (EggIdletime *idletime)
{
	g_return_val_if_fail (EGG_IS_IDLETIME (idletime), 0);

#ifdef HAVE_X11
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_X11_DISPLAY (gdk_display_get_default ()) &&
	    idletime->priv->idle_counter) {
		XSyncValue value;
		XSyncQueryCounter (idletime->priv->dpy, idletime->priv->idle_counter, &value);
		return egg_idletime_xsyncvalue_to_int64 (value);
	}
#endif
#ifdef HAVE_WAYLAND
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()) &&
	    idletime->priv->wayland_is_idle &&
	    idletime->priv->idle_timer) {
		return (gint64)(g_timer_elapsed (idletime->priv->idle_timer, NULL) * 1000.0);
	}
#endif
	return 0;
}

#ifdef HAVE_WAYLAND

/**
 * egg_idletime_wayland_registry_global_cb:
 */
static void
egg_idletime_wayland_registry_global_cb (void *data, struct wl_registry *registry,
					 uint32_t name, const char *interface,
					 uint32_t version)
{
	EggIdletime *idletime = EGG_IDLETIME (data);

	if (g_strcmp0 (interface, "ext_idle_notifier_v1") == 0) {
		idletime->priv->idle_notifier = wl_registry_bind (registry, name,
			&ext_idle_notifier_v1_interface, MIN (version, 2u));
		g_debug ("bound ext_idle_notifier_v1 (version %u)", MIN (version, 2u));
	} else if (g_strcmp0 (interface, "wl_seat") == 0) {
		if (idletime->priv->seat == NULL) {
			idletime->priv->seat = wl_registry_bind (registry, name,
				&wl_seat_interface, MIN (version, 1u));
		}
	}
}

static void
egg_idletime_wayland_registry_global_remove_cb (void *data, struct wl_registry *registry,
						uint32_t name)
{
}

static const struct wl_registry_listener egg_idletime_wayland_registry_listener = {
	.global = egg_idletime_wayland_registry_global_cb,
	.global_remove = egg_idletime_wayland_registry_global_remove_cb,
};

#endif /* HAVE_WAYLAND */

/**
 * egg_idletime_init:
 **/
static void
egg_idletime_init (EggIdletime *idletime)
{
	idletime->priv = egg_idletime_get_instance_private (idletime);

	idletime->priv->reset_set = FALSE;
	idletime->priv->array = g_ptr_array_new ();

#ifdef HAVE_WAYLAND
	idletime->priv->wayland_notifications = NULL;
	idletime->priv->wayland_is_idle = FALSE;
	idletime->priv->idle_timer = NULL;
#endif

#ifdef HAVE_X11
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		int sync_error;
		int ncounters;
		XSyncSystemCounter *counters;
		EggIdletimeAlarm *alarm;
		gint i;

		idletime->priv->idle_counter = None;
		idletime->priv->sync_event = 0;
		idletime->priv->dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default());

		/* get the sync event */
		if (!XSyncQueryExtension (idletime->priv->dpy, &idletime->priv->sync_event, &sync_error)) {
			g_warning ("No Sync extension.");
			return;
		}

		/* gtk_init should do XSyncInitialize for us */
		counters = XSyncListSystemCounters (idletime->priv->dpy, &ncounters);
		for (i=0; i < ncounters && !idletime->priv->idle_counter; i++) {
			if (strcmp(counters[i].name, "IDLETIME") == 0)
				idletime->priv->idle_counter = counters[i].counter;
		}
		XSyncFreeSystemCounterList (counters);

		/* arh. we don't have IDLETIME support */
		if (!idletime->priv->idle_counter) {
			g_warning ("No idle counter.");
			return;
		}

		/* catch the timer alarm */
		gdk_window_add_filter (NULL, egg_idletime_event_filter_cb, idletime);

		/* create a reset alarm */
		alarm = egg_idletime_alarm_new (idletime, 0);
		g_ptr_array_add (idletime->priv->array, alarm);
		return;
	}
#endif
#ifdef HAVE_WAYLAND
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
		struct wl_display *display;
		struct wl_registry *registry;

		idletime->priv->wayland_notifications = g_ptr_array_new_with_free_func (g_free);
		idletime->priv->idle_timer = g_timer_new ();

		display = gdk_wayland_display_get_wl_display (gdk_display_get_default ());
		if (display == NULL) {
			g_warning ("Cannot get Wayland display");
			return;
		}

		registry = wl_display_get_registry (display);
		if (registry == NULL) {
			g_warning ("Cannot get Wayland registry");
			return;
		}

		wl_registry_add_listener (registry, &egg_idletime_wayland_registry_listener, idletime);
		wl_display_roundtrip (display);

		if (idletime->priv->idle_notifier == NULL) {
			g_debug ("Compositor does not support ext_idle_notifier_v1");
		}
		if (idletime->priv->seat == NULL) {
			g_warning ("No wl_seat available");
		}
		return;
	}
#endif
	g_debug ("No idle tracking available for current display");
}

/**
 * egg_idletime_alarm_set:
 */
gboolean
egg_idletime_alarm_set (EggIdletime *idletime, guint id, guint timeout)
{
	g_return_val_if_fail (EGG_IS_IDLETIME (idletime), FALSE);
	g_return_val_if_fail (id != 0, FALSE);
	g_return_val_if_fail (timeout != 0, FALSE);

#ifdef HAVE_X11
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		EggIdletimeAlarm *alarm;

		/* see if we already created an alarm with this ID */
		alarm = egg_idletime_alarm_find_id (idletime, id);
		if (alarm == NULL) {
			/* create a new alarm */
			alarm = egg_idletime_alarm_new (idletime, id);

			/* add to array */
			g_ptr_array_add (idletime->priv->array, alarm);
		}

		/* set the timeout */
		XSyncIntToValue (&alarm->timeout, (gint)timeout);

		/* set, and start the timer */
		egg_idletime_xsync_alarm_set (idletime, alarm, EGG_IDLETIME_ALARM_TYPE_POSITIVE);
		return TRUE;
	}
#endif
#ifdef HAVE_WAYLAND
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
		EggIdletimeWaylandNotification *wn;

		if (idletime->priv->idle_notifier == NULL) {
			g_warning ("EggIdletime: idle_notifier not available");
			return FALSE;
		}
		if (idletime->priv->seat == NULL) {
			g_warning ("EggIdletime: no Wayland seat available");
			return FALSE;
		}

		/* see if we already created a notification with this ID */
		wn = egg_idletime_alarm_wayland_find_id (idletime, id);
		if (wn != NULL) {
			if (wn->notification)
				ext_idle_notification_v1_destroy (wn->notification);
			wn->timeout = timeout;
		} else {
			wn = g_new0 (EggIdletimeWaylandNotification, 1);
			wn->id = id;
			wn->timeout = timeout;
			g_ptr_array_add (idletime->priv->wayland_notifications, wn);
		}

		wn->notification = ext_idle_notifier_v1_get_input_idle_notification (
			idletime->priv->idle_notifier, timeout, idletime->priv->seat);
		ext_idle_notification_v1_add_listener (wn->notification,
			&egg_idletime_wayland_notification_listener, idletime);

		return TRUE;
	}
#endif
	return FALSE;
}

/**
 * egg_idletime_alarm_remove:
 */
gboolean
egg_idletime_alarm_remove (EggIdletime *idletime, guint id)
{
	g_return_val_if_fail (EGG_IS_IDLETIME (idletime), FALSE);

#ifdef HAVE_X11
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		EggIdletimeAlarm *alarm;

		alarm = egg_idletime_alarm_find_id (idletime, id);
		if (alarm == NULL)
			return FALSE;
		return egg_idletime_alarm_free (idletime, alarm);
	}
#endif
#ifdef HAVE_WAYLAND
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
		EggIdletimeWaylandNotification *wn;

		wn = egg_idletime_alarm_wayland_find_id (idletime, id);
		if (wn == NULL)
			return FALSE;

		if (wn->notification)
			ext_idle_notification_v1_destroy (wn->notification);
		g_ptr_array_remove (idletime->priv->wayland_notifications, wn);
		g_free (wn);
		return TRUE;
	}
#endif
	return FALSE;
}

/**
 * egg_idletime_alarm_reset_all:
 */
void
egg_idletime_alarm_reset_all (EggIdletime *idletime)
{
	g_return_if_fail (EGG_IS_IDLETIME (idletime));

#ifdef HAVE_X11
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		guint i;
		EggIdletimeAlarm *alarm;

		if (!idletime->priv->reset_set)
			return;

		/* reset all the alarms (except the reset alarm) to their timeouts */
		for (i=1; i<idletime->priv->array->len; i++) {
			alarm = g_ptr_array_index (idletime->priv->array, i);
			egg_idletime_xsync_alarm_set (idletime, alarm, EGG_IDLETIME_ALARM_TYPE_POSITIVE);
		}

		/* set the reset alarm to be disabled */
		alarm = g_ptr_array_index (idletime->priv->array, 0);
		egg_idletime_xsync_alarm_set (idletime, alarm, EGG_IDLETIME_ALARM_TYPE_DISABLED);

		/* emit signal so say we've reset all timers */
		g_signal_emit (idletime, signals [SIGNAL_RESET], 0);

		/* we need to be reset again on the next event */
		idletime->priv->reset_set = FALSE;
		return;
	}
#endif
#ifdef HAVE_WAYLAND
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
		g_signal_emit (idletime, signals [SIGNAL_RESET], 0);
		return;
	}
#endif
}

/**
 * egg_idletime_finalize:
 **/
static void
egg_idletime_finalize (GObject *object)
{
	guint i;
	EggIdletime *idletime;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EGG_IS_IDLETIME (object));

	idletime = EGG_IDLETIME (object);
	idletime->priv = egg_idletime_get_instance_private (idletime);

#ifdef HAVE_X11
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		EggIdletimeAlarm *alarm;

		/* free all counters, including reset counter */
		for (i=0; i<idletime->priv->array->len; i++) {
			alarm = g_ptr_array_index (idletime->priv->array, i);
			if (alarm->xalarm)
				XSyncDestroyAlarm (idletime->priv->dpy, alarm->xalarm);
			g_object_unref (alarm->idletime);
			g_free (alarm);
		}
	}
#endif
#ifdef HAVE_WAYLAND
	if (idletime->priv->wayland_notifications != NULL) {
		EggIdletimeWaylandNotification *wn;

		for (i = 0; i < idletime->priv->wayland_notifications->len; i++) {
			wn = g_ptr_array_index (idletime->priv->wayland_notifications, i);
			if (wn->notification)
				ext_idle_notification_v1_destroy (wn->notification);
		}
		g_ptr_array_free (idletime->priv->wayland_notifications, TRUE);
	}
	if (idletime->priv->idle_notifier)
		ext_idle_notifier_v1_destroy (idletime->priv->idle_notifier);
	if (idletime->priv->idle_timer)
		g_timer_destroy (idletime->priv->idle_timer);
#endif
	g_ptr_array_free (idletime->priv->array, TRUE);

	G_OBJECT_CLASS (egg_idletime_parent_class)->finalize (object);
}

/**
 * egg_idletime_new:
 **/
EggIdletime *
egg_idletime_new (void)
{
	if (egg_idletime_object != NULL) {
		g_object_ref (egg_idletime_object);
	} else {
		egg_idletime_object = g_object_new (EGG_IDLETIME_TYPE, NULL);
		g_object_add_weak_pointer (egg_idletime_object, &egg_idletime_object);
	}
	return EGG_IDLETIME (egg_idletime_object);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

static void
egg_test_egg_idletime_wait (guint time_ms)
{
	GTimer *ltimer = g_timer_new ();
	gfloat goal = time_ms / (gfloat) 1000.0f;
	do {
		g_main_context_iteration (NULL, FALSE);
	} while (g_timer_elapsed (ltimer, NULL) < goal);
	g_timer_destroy (ltimer);
}

static guint last_alarm = 0;
static guint event_time;
GTimer *timer;

static void
gpm_alarm_expired_cb (EggIdletime *idletime, guint alarm, gpointer data)
{
	last_alarm = alarm;
	event_time = g_timer_elapsed (timer, NULL) * (gfloat) 1000.0f;
//	g_print ("[evt %i in %ims]\n", alarm, event_time);
}

static void
wait_until_alarm (void)
{
	g_print ("*****************************\n");
	g_print ("*** DO NOT MOVE THE MOUSE ***\n");
	g_print ("*****************************\n");
	while (last_alarm == 0)
		g_main_context_iteration (NULL, FALSE);
}

static void
wait_until_reset (void)
{
	if (last_alarm == 0)
		return;
	g_print ("*****************************\n");
	g_print ("***     MOVE THE MOUSE    ***\n");
	g_print ("*****************************\n");
	while (last_alarm != 0)
		g_main_context_iteration (NULL, FALSE);
	egg_test_egg_idletime_wait (1000);
}

void
egg_idletime_test (gpointer data)
{
	EggIdletime *idletime;
	gboolean ret;
	guint i;
	EggTest *test = (EggTest *) data;

	if (egg_test_start (test, "EggIdletime") == FALSE)
		return;

	timer = g_timer_new ();
	gdk_init (NULL, NULL);

	/* warn */

	g_timer_start (timer);
	/************************************************************/
	egg_test_title (test, "check to see if delay works as expected");
	egg_test_egg_idletime_wait (2000);
	event_time = g_timer_elapsed (timer, NULL) * (gfloat) 1000.0f;
	if (event_time > 1800 && event_time < 2200) {
		egg_test_success (test, "time %i~=%i", 2000, event_time);
	} else {
		egg_test_failed (test, "time not the same! %i != %i", event_time, 2000);
	}

	/************************************************************/
	egg_test_title (test, "make sure we get a non null device");
	idletime = egg_idletime_new ();
	if (idletime != NULL) {
		egg_test_success (test, "got EggIdletime");
	} else {
		egg_test_failed (test, "could not get EggIdletime");
	}
	g_signal_connect (idletime, "alarm-expired",
			  G_CALLBACK (gpm_alarm_expired_cb), NULL);

	/************************************************************/
	egg_test_title (test, "check if we are alarm zero with no alarms");
	if (last_alarm == 0) {
		egg_test_success (test, NULL);
	} else {
		egg_test_failed (test, "alarm %i set!", last_alarm);
	}

	/************************************************************/
	egg_test_title (test, "check if we can set an reset alarm");
	ret = egg_idletime_alarm_set (idletime, 0, 100);
	if (!ret) {
		egg_test_success (test, "ignored reset alarm");
	} else {
		egg_test_failed (test, "did not ignore reset alarm");
	}

	/************************************************************/
	egg_test_title (test, "check if we can set an alarm timeout of zero");
	ret = egg_idletime_alarm_set (idletime, 999, 0);
	if (!ret) {
		egg_test_success (test, "ignored invalid alarm");
	} else {
		egg_test_failed (test, "did not ignore invalid alarm");
	}

	/************************************************************/
	g_timer_start (timer);
	egg_test_title (test, "check if we can set an alarm");
	ret = egg_idletime_alarm_set (idletime, 101, 5000);
	if (ret) {
		egg_test_success (test, "set alarm okay");
	} else {
		egg_test_failed (test, "could not set alarm");
	}

	egg_idletime_alarm_set (idletime, 101, 5000);
	wait_until_alarm ();

	/* loop this two times */
	for (i=0; i<2; i++) {
		/* just let it time out, and wait for human input */
		wait_until_reset ();
		g_timer_start (timer);

		/************************************************************/
		g_timer_start (timer);
		egg_test_title (test, "check if we can set an alarm");
		ret = egg_idletime_alarm_set (idletime, 101, 5000);
		if (ret) {
			egg_test_success (test, "set alarm 5000ms okay");
		} else {
			egg_test_failed (test, "could not set alarm 5000ms");
		}

		/* wait for alarm to go off */
		wait_until_alarm ();
		g_timer_start (timer);

		/************************************************************/
		egg_test_title (test, "check if correct alarm has gone off");
		if (last_alarm == 101) {
			egg_test_success (test, "correct alarm");
		} else {
			egg_test_failed (test, "alarm %i set!", last_alarm);
		}

		/************************************************************/
		egg_test_title (test, "check if alarm has gone off in correct time");
		if (event_time > 3000 && event_time < 6000) {
			egg_test_success (test, "correct, timeout ideally %ims (we did after %ims)", 5000, event_time);
		} else {
			egg_test_failed (test, "alarm %i did not timeout correctly !", last_alarm);
		}
	}

	/* just let it time out, and wait for human input */
	wait_until_reset ();
	g_timer_start (timer);

	/************************************************************/
	g_timer_start (timer);
	egg_test_title (test, "check if we can set an existing alarm");
	ret = egg_idletime_alarm_set (idletime, 101, 10000);
	if (ret) {
		egg_test_success (test, "set alarm 10000ms okay");
	} else {
		egg_test_failed (test, "could not set alarm 10000ms");
	}

	/* wait for alarm to go off */
	wait_until_alarm ();
	g_timer_start (timer);

	/************************************************************/
	egg_test_title (test, "check if alarm has gone off in the old time");
	if (event_time > 5000) {
		egg_test_success (test, "last timeout value used");
	} else {
		egg_test_failed (test, "incorrect timeout used %ims", event_time);
	}

	/************************************************************/
	egg_test_title (test, "check if we can remove an invalid alarm");
	ret = egg_idletime_alarm_remove (idletime, 202);
	if (!ret) {
		egg_test_success (test, "ignored invalid alarm");
	} else {
		egg_test_failed (test, "removed invalid alarm");
	}

	/************************************************************/
	egg_test_title (test, "check if we can remove an valid alarm");
	ret = egg_idletime_alarm_remove (idletime, 101);
	if (ret) {
		egg_test_success (test, "removed valid alarm");
	} else {
		egg_test_failed (test, "failed to remove valid alarm");
	}

	g_timer_destroy (timer);
	g_object_unref (idletime);

	egg_test_end (test);
}

#endif
