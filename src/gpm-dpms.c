/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2006-2009 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <gdk/gdk.h>

#ifdef HAVE_X11
#include <gdk/gdkx.h>
#include <X11/Xproto.h>
#include <X11/extensions/dpms.h>
#endif /* HAVE_X11 */

#ifdef HAVE_WAYLAND
#include <gdk/gdkwayland.h>
#include <wayland-client.h>
#include "wlr-output-power-management-unstable-v1-client.h"
#endif /* HAVE_WAYLAND */

#include "gpm-dpms.h"

static void   gpm_dpms_finalize  (GObject   *object);

/* until we get a nice event-emitting DPMS extension, we have to poll... */
#define GPM_DPMS_POLL_TIME	10

#ifdef HAVE_WAYLAND
typedef struct {
	struct zwlr_output_power_v1 *power;
	uint32_t name;
	GpmDpmsMode mode;
} GpmDpmsWaylandOutput;
#endif

struct GpmDpmsPrivate
{
	gboolean		 dpms_capable;
	GpmDpmsMode		 mode;
	guint			 timer_id;
#ifdef HAVE_X11
	Display			*display;
#endif
#ifdef HAVE_WAYLAND
	struct zwlr_output_power_manager_v1 *power_manager;
	GPtrArray *wayland_outputs;
#endif
};

enum {
	MODE_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_dpms_object = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (GpmDpms, gpm_dpms, G_TYPE_OBJECT)

/**
 * gpm_dpms_error_quark:
 **/
GQuark
gpm_dpms_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gpm_dpms_error");
	return quark;
}

#ifdef HAVE_X11

/**
 * gpm_dpms_x11_get_mode:
 **/
static gboolean
gpm_dpms_x11_get_mode (GpmDpms *dpms, GpmDpmsMode *mode, GError **error)
{
	GpmDpmsMode result;
	BOOL enabled = FALSE;
	CARD16 state;

	if (dpms->priv->dpms_capable == FALSE) {
		/* Server or monitor can't DPMS -- assume the monitor is on. */
		result = GPM_DPMS_MODE_ON;
		goto out;
	}

	DPMSInfo (dpms->priv->display, &state, &enabled);
	if (!enabled) {
		/* Server says DPMS is disabled -- so the monitor is on. */
		result = GPM_DPMS_MODE_ON;
		goto out;
	}

	switch (state) {
	case DPMSModeOn:
		result = GPM_DPMS_MODE_ON;
		break;
	case DPMSModeStandby:
		result = GPM_DPMS_MODE_STANDBY;
		break;
	case DPMSModeSuspend:
		result = GPM_DPMS_MODE_SUSPEND;
		break;
	case DPMSModeOff:
		result = GPM_DPMS_MODE_OFF;
		break;
	default:
		result = GPM_DPMS_MODE_ON;
		break;
	}
out:
	if (mode)
		*mode = result;
	return TRUE;
}

/**
 * gpm_dpms_x11_set_mode:
 **/
static gboolean
gpm_dpms_x11_set_mode (GpmDpms *dpms, GpmDpmsMode mode, GError **error)
{
	GpmDpmsMode current_mode;
	CARD16 state;
	CARD16 current_state;
	BOOL current_enabled;

	if (!dpms->priv->dpms_capable) {
		g_debug ("not DPMS capable");
		g_set_error (error, GPM_DPMS_ERROR, GPM_DPMS_ERROR_GENERAL,
			     "Display is not DPMS capable");
		return FALSE;
	}

	if (!DPMSInfo (dpms->priv->display, &current_state, &current_enabled)) {
		g_debug ("couldn't get DPMS info");
		g_set_error (error, GPM_DPMS_ERROR, GPM_DPMS_ERROR_GENERAL,
			     "Unable to get DPMS state");
		return FALSE;
	}

	if (!current_enabled) {
		g_debug ("DPMS not enabled");
		g_set_error (error, GPM_DPMS_ERROR, GPM_DPMS_ERROR_GENERAL,
			     "DPMS is not enabled");
		return FALSE;
	}

	switch (mode) {
	case GPM_DPMS_MODE_ON:
		state = DPMSModeOn;
		break;
	case GPM_DPMS_MODE_STANDBY:
		state = DPMSModeStandby;
		break;
	case GPM_DPMS_MODE_SUSPEND:
		state = DPMSModeSuspend;
		break;
	case GPM_DPMS_MODE_OFF:
		state = DPMSModeOff;
		break;
	default:
		state = DPMSModeOn;
		break;
	}

	gpm_dpms_x11_get_mode (dpms, &current_mode, NULL);
	if (current_mode != mode) {
		if (! DPMSForceLevel (dpms->priv->display, state)) {
			g_set_error (error, GPM_DPMS_ERROR, GPM_DPMS_ERROR_GENERAL,
				     "Could not change DPMS mode");
			return FALSE;
		}
		XSync (dpms->priv->display, FALSE);
	}

	return TRUE;
}

/**
 * gpm_dpms_clear_timeouts:
 **/
static gboolean
gpm_dpms_clear_timeouts (GpmDpms *dpms)
{
	gboolean ret = FALSE;

	/* never going to work */
	if (!dpms->priv->dpms_capable) {
		g_debug ("not DPMS capable");
		goto out;
	}

	g_debug ("set timeouts to zero");
	ret = DPMSSetTimeouts (dpms->priv->display, 0, 0, 0);

out:
	return ret;
}

#endif /* HAVE_X11 */

#ifdef HAVE_WAYLAND

static void
gpm_dpms_wayland_output_mode_cb (void *data,
				 struct zwlr_output_power_v1 *power,
				 uint32_t mode)
{
	GpmDpmsWaylandOutput *output = data;
	GpmDpmsMode new_mode;

	switch (mode) {
	case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
		new_mode = GPM_DPMS_MODE_OFF;
		break;
	case ZWLR_OUTPUT_POWER_V1_MODE_ON:
	default:
		new_mode = GPM_DPMS_MODE_ON;
		break;
	}

	if (new_mode != output->mode) {
		output->mode = new_mode;
	}
}

static void
gpm_dpms_wayland_output_failed_cb (void *data,
				   struct zwlr_output_power_v1 *power)
{
	GpmDpmsWaylandOutput *output = data;
	g_debug ("wlr-output-power failed for output %u", output->name);
}

static const struct zwlr_output_power_v1_listener
gpm_dpms_wayland_power_listener = {
	.mode = gpm_dpms_wayland_output_mode_cb,
	.failed = gpm_dpms_wayland_output_failed_cb,
};

static GpmDpmsWaylandOutput *
gpm_dpms_wayland_output_find (GpmDpms *dpms, uint32_t name)
{
	guint i;
	for (i = 0; i < dpms->priv->wayland_outputs->len; i++) {
		GpmDpmsWaylandOutput *o = g_ptr_array_index (dpms->priv->wayland_outputs, i);
		if (o->name == name)
			return o;
	}
	return NULL;
}

static void
gpm_dpms_wayland_registry_global_cb (void *data, struct wl_registry *registry,
				     uint32_t name, const char *interface,
				     uint32_t version)
{
	GpmDpms *dpms = data;

	if (g_strcmp0 (interface, "zwlr_output_power_manager_v1") == 0) {
		dpms->priv->power_manager = wl_registry_bind (registry, name,
			&zwlr_output_power_manager_v1_interface, 1);
		g_debug ("bound zwlr_output_power_manager_v1");
	} else if (g_strcmp0 (interface, "wl_output") == 0) {
		struct wl_output *output;
		GpmDpmsWaylandOutput *wo;

		wo = gpm_dpms_wayland_output_find (dpms, name);
		if (wo != NULL)
			return;

		output = wl_registry_bind (registry, name,
					   &wl_output_interface, 1);
		if (dpms->priv->power_manager == NULL)
			return;

		wo = g_new0 (GpmDpmsWaylandOutput, 1);
		wo->name = name;
		wo->mode = GPM_DPMS_MODE_ON;
		wo->power = zwlr_output_power_manager_v1_get_output_power (
			dpms->priv->power_manager, output);
		zwlr_output_power_v1_add_listener (wo->power,
			&gpm_dpms_wayland_power_listener, wo);
		g_ptr_array_add (dpms->priv->wayland_outputs, wo);
		g_debug ("added zwlr_output_power_v1 for output %u", name);
	}
}

static void
gpm_dpms_wayland_registry_global_remove_cb (void *data,
					    struct wl_registry *registry,
					    uint32_t name)
{
	GpmDpms *dpms = data;
	GpmDpmsWaylandOutput *wo = gpm_dpms_wayland_output_find (dpms, name);
	if (wo == NULL)
		return;
	if (wo->power)
		zwlr_output_power_v1_destroy (wo->power);
	g_ptr_array_remove (dpms->priv->wayland_outputs, wo);
	g_free (wo);
	g_debug ("removed zwlr_output_power_v1 for output %u", name);
}

static const struct wl_registry_listener gpm_dpms_wayland_registry_listener = {
	.global = gpm_dpms_wayland_registry_global_cb,
	.global_remove = gpm_dpms_wayland_registry_global_remove_cb,
};

static gboolean
gpm_dpms_wayland_set_mode (GpmDpms *dpms, GpmDpmsMode mode, GError **error)
{
	guint i;
	uint32_t wlr_mode;

	if (dpms->priv->wayland_outputs->len == 0) {
		g_set_error (error, GPM_DPMS_ERROR, GPM_DPMS_ERROR_GENERAL,
			     "No Wayland outputs available");
		return FALSE;
	}

	switch (mode) {
	case GPM_DPMS_MODE_OFF:
		wlr_mode = ZWLR_OUTPUT_POWER_V1_MODE_OFF;
		break;
	case GPM_DPMS_MODE_ON:
	case GPM_DPMS_MODE_STANDBY:
	case GPM_DPMS_MODE_SUSPEND:
	default:
		wlr_mode = ZWLR_OUTPUT_POWER_V1_MODE_ON;
		break;
	}

	for (i = 0; i < dpms->priv->wayland_outputs->len; i++) {
		GpmDpmsWaylandOutput *wo = g_ptr_array_index (dpms->priv->wayland_outputs, i);
		zwlr_output_power_v1_set_mode (wo->power, wlr_mode);
	}

	return TRUE;
}

#endif /* HAVE_WAYLAND */

/**
 * gpm_dpms_set_mode:
 **/
gboolean
gpm_dpms_set_mode (GpmDpms *dpms, GpmDpmsMode mode, GError **error)
{
	g_return_val_if_fail (GPM_IS_DPMS (dpms), FALSE);

	if (mode == GPM_DPMS_MODE_UNKNOWN) {
		g_debug ("mode unknown");
		g_set_error (error, GPM_DPMS_ERROR, GPM_DPMS_ERROR_GENERAL,
			     "Unknown DPMS mode");
		return FALSE;
	}

#ifdef HAVE_X11
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		return gpm_dpms_x11_set_mode (dpms, mode, error);
	}
#endif
#ifdef HAVE_WAYLAND
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
		return gpm_dpms_wayland_set_mode (dpms, mode, error);
	}
#endif
	g_set_error (error, GPM_DPMS_ERROR, GPM_DPMS_ERROR_GENERAL,
		     "DPMS not supported on this display");
	return FALSE;
}

/**
 * gpm_dpms_get_mode:
 **/
gboolean
gpm_dpms_get_mode (GpmDpms *dpms, GpmDpmsMode *mode, GError **error)
{
	g_return_val_if_fail (GPM_IS_DPMS (dpms), FALSE);

#ifdef HAVE_X11
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		return gpm_dpms_x11_get_mode (dpms, mode, error);
	}
#endif
#ifdef HAVE_WAYLAND
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
		GpmDpmsMode result = GPM_DPMS_MODE_ON;
		guint i;
		gboolean all_off = TRUE;

		for (i = 0; i < dpms->priv->wayland_outputs->len; i++) {
			GpmDpmsWaylandOutput *wo = g_ptr_array_index (dpms->priv->wayland_outputs, i);
			if (wo->mode != GPM_DPMS_MODE_OFF) {
				all_off = FALSE;
				break;
			}
		}
		if (all_off && dpms->priv->wayland_outputs->len > 0)
			result = GPM_DPMS_MODE_OFF;
		if (mode)
			*mode = result;
		return TRUE;
	}
#endif
	if (mode)
		*mode = GPM_DPMS_MODE_ON;
	return TRUE;
}

/**
 * gpm_dpms_poll_mode_cb:
 **/
static gboolean
gpm_dpms_poll_mode_cb (GpmDpms *dpms)
{
#ifdef HAVE_X11
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		gboolean ret;
		GpmDpmsMode mode;
		GError *error = NULL;

		ret = gpm_dpms_x11_get_mode (dpms, &mode, &error);
		if (!ret) {
			g_clear_error (&error);
			return TRUE;
		}

		if (mode != dpms->priv->mode) {
			dpms->priv->mode = mode;
			g_signal_emit (dpms, signals [MODE_CHANGED], 0, mode);
		}
		return TRUE;
	}
#endif
	return TRUE;
}

/**
 * gpm_dpms_class_init:
 **/
static void
gpm_dpms_class_init (GpmDpmsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gpm_dpms_finalize;

	signals [MODE_CHANGED] =
		g_signal_new ("mode-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmDpmsClass, mode_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
}

/**
 * gpm_dpms_init:
 **/
static void
gpm_dpms_init (GpmDpms *dpms)
{
	dpms->priv = gpm_dpms_get_instance_private (dpms);

	dpms->priv->dpms_capable = FALSE;
	dpms->priv->mode = GPM_DPMS_MODE_UNKNOWN;
	dpms->priv->timer_id = 0;

#ifdef HAVE_X11
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		/* DPMSCapable() can never change for a given display */
		dpms->priv->display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default());
		dpms->priv->dpms_capable = DPMSCapable (dpms->priv->display);
		dpms->priv->timer_id = g_timeout_add_seconds (GPM_DPMS_POLL_TIME, (GSourceFunc)gpm_dpms_poll_mode_cb, dpms);
		g_source_set_name_by_id (dpms->priv->timer_id, "[GpmDpms] poll");

		/* ensure we clear the default timeouts (Standby: 1200s, Suspend: 1800s, Off: 2400s) */
		gpm_dpms_clear_timeouts (dpms);
		return;
	}
#endif
#ifdef HAVE_WAYLAND
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
		struct wl_display *display;
		struct wl_registry *registry;

		dpms->priv->wayland_outputs = g_ptr_array_new_with_free_func (g_free);

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

		wl_registry_add_listener (registry,
			&gpm_dpms_wayland_registry_listener, dpms);
		wl_display_roundtrip (display);

		if (dpms->priv->power_manager == NULL) {
			g_debug ("Compositor does not support zwlr_output_power_manager_v1");
		}

		dpms->priv->timer_id = g_timeout_add_seconds (GPM_DPMS_POLL_TIME,
			(GSourceFunc) gpm_dpms_poll_mode_cb, dpms);
		g_source_set_name_by_id (dpms->priv->timer_id, "[GpmDpms] poll");
		return;
	}
#endif
	g_debug ("No DPMS backend available for current display");
}

/**
 * gpm_dpms_finalize:
 **/
static void
gpm_dpms_finalize (GObject *object)
{
	GpmDpms *dpms;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_DPMS (object));

	dpms = GPM_DPMS (object);

	g_return_if_fail (dpms->priv != NULL);

	if (dpms->priv->timer_id != 0) {
		g_source_remove (dpms->priv->timer_id);
		dpms->priv->timer_id = 0;
	}

#ifdef HAVE_WAYLAND
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
		guint i;
		for (i = 0; i < dpms->priv->wayland_outputs->len; i++) {
			GpmDpmsWaylandOutput *wo = g_ptr_array_index (dpms->priv->wayland_outputs, i);
			if (wo->power)
				zwlr_output_power_v1_destroy (wo->power);
		}
		g_ptr_array_free (dpms->priv->wayland_outputs, TRUE);
		if (dpms->priv->power_manager)
			zwlr_output_power_manager_v1_destroy (dpms->priv->power_manager);
	}
#endif

	G_OBJECT_CLASS (gpm_dpms_parent_class)->finalize (object);
}

/**
 * gpm_dpms_new:
 **/
GpmDpms *
gpm_dpms_new (void)
{
	if (gpm_dpms_object != NULL) {
		g_object_ref (gpm_dpms_object);
	} else {
		gpm_dpms_object = g_object_new (GPM_TYPE_DPMS, NULL);
		g_object_add_weak_pointer (gpm_dpms_object, &gpm_dpms_object);
	}
	return GPM_DPMS (gpm_dpms_object);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
gpm_dpms_test (gpointer data)
{
	GpmDpms *dpms;
	gboolean ret;
	GError *error = NULL;
	EggTest *test = (EggTest *) data;

	if (!egg_test_start (test, "GpmDpms"))
		return;

	/************************************************************/
	egg_test_title (test, "get object");
	dpms = gpm_dpms_new ();
	if (dpms != NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "got no object");

	/************************************************************/
	egg_test_title (test, "set on");
	ret = gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_ON, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed: %s", error->message);

	g_usleep (2*1000*1000);

	/************************************************************/
	egg_test_title (test, "set STANDBY");
	ret = gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_STANDBY, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed: %s", error->message);

	g_usleep (2*1000*1000);

	/************************************************************/
	egg_test_title (test, "set SUSPEND");
	ret = gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_SUSPEND, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed: %s", error->message);

	g_usleep (2*1000*1000);

	/************************************************************/
	egg_test_title (test, "set OFF");
	ret = gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_OFF, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed: %s", error->message);

	g_usleep (2*1000*1000);

	/************************************************************/
	egg_test_title (test, "set on");
	ret = gpm_dpms_set_mode (dpms, GPM_DPMS_MODE_ON, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed: %s", error->message);

	g_usleep (2*1000*1000);

	g_object_unref (dpms);

	egg_test_end (test);
}

#endif
