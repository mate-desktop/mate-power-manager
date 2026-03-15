/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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

#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include "gpm-screensaver.h"
#include "gpm-common.h"

static void     gpm_screensaver_finalize   (GObject		*object);

#define GS_LISTENER_SERVICE	"org.mate.ScreenSaver"
#define GS_LISTENER_PATH	"/"
#define GS_LISTENER_INTERFACE	"org.mate.ScreenSaver"

struct GpmScreensaverPrivate
{
	GDBusProxy		*proxy;
};

static gpointer gpm_screensaver_object = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (GpmScreensaver, gpm_screensaver, G_TYPE_OBJECT)

/**
 * gpm_screensaver_lock
 * @screensaver: This class instance
 * Return value: Success value
 **/
gboolean
gpm_screensaver_lock (GpmScreensaver *screensaver)
{
	GError *error = NULL;
	GVariant *result;
	guint sleepcount = 0;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (screensaver->priv->proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	g_debug ("doing mate-screensaver lock");
	result = g_dbus_proxy_call_sync (screensaver->priv->proxy,
					 "Lock",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	if (error != NULL) {
		g_warning ("Lock failed: %s", error->message);
		g_error_free (error);
		return FALSE;
	}
	if (result != NULL)
		g_variant_unref (result);

	/* When we send the Lock signal to g-ss it takes maybe a second
	   or so to fade the screen and lock. If we suspend mid fade then on
	   resume the X display is still present for a split second
	   (since fade is gamma) and as such it can leak information.
	   Instead we wait until g-ss reports running and thus blanked
	   solidly before we continue from the screensaver_lock action.
	   The interior of g-ss is async, so we cannot get the dbus method
	   to block until lock is complete. */
	while (! gpm_screensaver_check_running (screensaver)) {
		/* Sleep for 1/10s */
		g_usleep (1000 * 100);
		if (sleepcount++ > 50) {
			g_debug ("timeout waiting for mate-screensaver");
			break;
		}
	}

	return TRUE;
}

/**
 * gpm_screensaver_add_throttle:
 * @screensaver: This class instance
 * @reason:      The reason for throttling
 * Return value: Success value, or zero for failure
 **/
guint
gpm_screensaver_add_throttle (GpmScreensaver *screensaver,
			      const char     *reason)
{
	GError  *error = NULL;
	GVariant *result;
	gboolean ret;
	guint32  cookie;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), 0);
	g_return_val_if_fail (reason != NULL, 0);

	if (screensaver->priv->proxy == NULL) {
		g_warning ("not connected");
		return 0;
	}

	result = g_dbus_proxy_call_sync (screensaver->priv->proxy,
					 "Throttle",
					 g_variant_new ("(ss)", "Power screensaver", reason),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	ret = (result != NULL);
	if (result != NULL) {
		g_variant_get (result, "(u)", &cookie);
		g_variant_unref (result);
	}
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		g_warning ("Throttle failed!");
		return 0;
	}

	g_debug ("adding throttle reason: '%s': id %u", reason, cookie);
	return cookie;
}

/**
 * gpm_screensaver_remove_throttle:
 **/
gboolean
gpm_screensaver_remove_throttle (GpmScreensaver *screensaver, guint cookie)
{
	gboolean ret;
	GError *error = NULL;
	GVariant *result;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (screensaver->priv->proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	g_debug ("removing throttle: id %u", cookie);
	result = g_dbus_proxy_call_sync (screensaver->priv->proxy,
					 "UnThrottle",
					 g_variant_new ("(u)", cookie),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	ret = (result != NULL);
	if (result != NULL)
		g_variant_unref (result);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		g_warning ("UnThrottle failed!");
		return FALSE;
	}

	return TRUE;
}

/**
 * gpm_screensaver_check_running:
 * @screensaver: This class instance
 * Return value: TRUE if mate-screensaver is running
 **/
gboolean
gpm_screensaver_check_running (GpmScreensaver *screensaver)
{
	gboolean temp = FALSE;
	GError *error = NULL;
	GVariant *result;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (screensaver->priv->proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	result = g_dbus_proxy_call_sync (screensaver->priv->proxy,
					 "GetActive",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	if (result != NULL) {
		g_variant_get (result, "(b)", &temp);
		g_variant_unref (result);
	}
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}

	return temp;
}

/**
 * gpm_screensaver_poke:
 * @screensaver: This class instance
 *
 * Pokes MATE Screensaver simulating hardware events. This displays the unlock
 * dialogue when we resume, so the user doesn't have to move the mouse or press
 * any key before the window comes up.
 **/
gboolean
gpm_screensaver_poke (GpmScreensaver *screensaver)
{
	GError *error = NULL;
	GVariant *result;

	g_return_val_if_fail (GPM_IS_SCREENSAVER (screensaver), FALSE);

	if (screensaver->priv->proxy == NULL) {
		g_warning ("not connected");
		return FALSE;
	}

	g_debug ("poke");
	result = g_dbus_proxy_call_sync (screensaver->priv->proxy,
					 "SimulateUserActivity",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	if (error != NULL) {
		g_warning ("SimulateUserActivity failed: %s", error->message);
		g_error_free (error);
		return FALSE;
	}
	if (result != NULL)
		g_variant_unref (result);

	return TRUE;
}

/**
 * gpm_screensaver_class_init:
 * @klass: This class instance
 **/
static void
gpm_screensaver_class_init (GpmScreensaverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_screensaver_finalize;
}

/**
 * gpm_screensaver_init:
 * @screensaver: This class instance
 **/
static void
gpm_screensaver_init (GpmScreensaver *screensaver)
{
	GError *error = NULL;

	screensaver->priv = gpm_screensaver_get_instance_private (screensaver);

	screensaver->priv->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
							   G_DBUS_PROXY_FLAGS_NONE,
							   NULL,
							   GS_LISTENER_SERVICE,
							   GS_LISTENER_PATH,
							   GS_LISTENER_INTERFACE,
							   NULL,
							   &error);
	if (error != NULL) {
		g_warning ("Could not create screensaver proxy: %s", error->message);
		g_error_free (error);
		screensaver->priv->proxy = NULL;
	}
}

/**
 * gpm_screensaver_finalize:
 * @object: This class instance
 **/
static void
gpm_screensaver_finalize (GObject *object)
{
	GpmScreensaver *screensaver;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SCREENSAVER (object));

	screensaver = GPM_SCREENSAVER (object);
	screensaver->priv = gpm_screensaver_get_instance_private (screensaver);

	if (screensaver->priv->proxy != NULL)
		g_object_unref (screensaver->priv->proxy);

	G_OBJECT_CLASS (gpm_screensaver_parent_class)->finalize (object);
}

/**
 * gpm_screensaver_new:
 * Return value: new GpmScreensaver instance.
 **/
GpmScreensaver *
gpm_screensaver_new (void)
{
	if (gpm_screensaver_object != NULL) {
		g_object_ref (gpm_screensaver_object);
	} else {
		gpm_screensaver_object = g_object_new (GPM_TYPE_SCREENSAVER, NULL);
		g_object_add_weak_pointer (gpm_screensaver_object, &gpm_screensaver_object);
	}
	return GPM_SCREENSAVER (gpm_screensaver_object);
}
/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
gpm_screensaver_test (gpointer data)
{
	GpmScreensaver *screensaver;
//	guint value;
	gboolean ret;
	EggTest *test = (EggTest *) data;

	if (egg_test_start (test, "GpmScreensaver") == FALSE)
		return;

	/************************************************************/
	egg_test_title (test, "make sure we get a non null screensaver");
	screensaver = gpm_screensaver_new ();
	egg_test_assert (test, (screensaver != NULL));

	/************************************************************/
	egg_test_title (test, "lock screensaver");
	ret = gpm_screensaver_lock (screensaver);
	egg_test_assert (test, ret);

	/************************************************************/
	egg_test_title (test, "poke screensaver");
	ret = gpm_screensaver_poke (screensaver);
	egg_test_assert (test, ret);

	g_object_unref (screensaver);

	egg_test_end (test);
}

#endif

