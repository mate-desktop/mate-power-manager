/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
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
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <gio/gio.h>
#include <glib/gi18n.h>

#ifdef WITH_KEYRING
#include <gnome-keyring.h>
#endif /* WITH_KEYRING */

#include "egg-debug.h"
#include "egg-console-kit.h"

#include "gpm-screensaver.h"
#include "gpm-common.h"
#include "gpm-control.h"
#include "gpm-networkmanager.h"

#define GPM_CONTROL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_CONTROL, GpmControlPrivate))

struct GpmControlPrivate
{
	GSettings		*settings;
};

enum {
	RESUME,
	SLEEP,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_control_object = NULL;

G_DEFINE_TYPE (GpmControl, gpm_control, G_TYPE_OBJECT)

/**
 * gpm_control_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gpm_control_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gpm_control_error");
	return quark;
}

/**
 * gpm_manager_systemd_shutdown:
 *
 * Shutdown the system using systemd-logind.
 *
 * Return value: fd, -1 on error
 **/
static gboolean
gpm_control_systemd_shutdown (void) {
	GError *error = NULL;
	GDBusProxy *proxy;
	GVariant *res = NULL;

	egg_debug ("Requesting systemd to shutdown");
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
					       NULL,
					       "org.freedesktop.login1",
					       "/org/freedesktop/login1",
					       "org.freedesktop.login1.Manager",
					       NULL,
					       &error );
	//append all our arguments
	if (proxy == NULL) {
		egg_error("Error connecting to dbus - %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	res = g_dbus_proxy_call_sync (proxy, "PowerOff",
				      g_variant_new( "(b)", FALSE),
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      &error
				      );
	if (error != NULL) {
		egg_error ("Error in dbus - %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	g_variant_unref(res);
	return TRUE;
}

/**
 * gpm_control_shutdown:
 * @control: This class instance
 *
 * Shuts down the computer
 **/
gboolean
gpm_control_shutdown (GpmControl *control, GError **error)
{
	gboolean ret;
	EggConsoleKit *console;

	if (LOGIND_RUNNING()) {
		ret = gpm_control_systemd_shutdown ();
	} else {
		console = egg_console_kit_new ();
		ret = egg_console_kit_stop (console, error);
		g_object_unref (console);
	}
	return ret;
}

/**
 * gpm_control_get_lock_policy:
 * @control: This class instance
 * @policy: The policy string.
 *
 * This function finds out if we should lock the screen when we do an
 * action. It is required as we can either use the mate-screensaver policy
 * or the custom policy. See the yelp file for more information.
 *
 * Return value: TRUE if we should lock.
 **/
gboolean
gpm_control_get_lock_policy (GpmControl *control, const gchar *policy)
{
	gboolean do_lock;
	gboolean use_ss_setting;
	const char * const *schemas;
	gboolean schema_exists;
	gint i;

	/* Check if the mate-screensaver schema exists before trying to read
	   the lock setting to prevent crashing. See GNOME bug #651225. */
	schemas = g_settings_list_schemas ();
	schema_exists = FALSE;
	for (i = 0; schemas[i] != NULL; i++) {
		if (g_strcmp0 (schemas[i], GS_SETTINGS_SCHEMA) == 0) {
			schema_exists = TRUE;
			break;
		}
	}

	/* This allows us to over-ride the custom lock settings set
	   with a system default set in mate-screensaver.
	   See bug #331164 for all the juicy details. :-) */
	use_ss_setting = g_settings_get_boolean (control->priv->settings, GPM_SETTINGS_LOCK_USE_SCREENSAVER);
	if (use_ss_setting && schema_exists) {
		GSettings *settings_ss;
		settings_ss = g_settings_new (GS_SETTINGS_SCHEMA);
		do_lock = g_settings_get_boolean (settings_ss, GS_SETTINGS_PREF_LOCK_ENABLED);
		egg_debug ("Using ScreenSaver settings (%i)", do_lock);
		g_object_unref (settings_ss);
	} else {
		do_lock = g_settings_get_boolean (control->priv->settings, policy);
		egg_debug ("Using custom locking settings (%i)", do_lock);
	}
	return do_lock;
}

/**
 * gpm_control_suspend:
 **/
gboolean
gpm_control_suspend (GpmControl *control, GError **error)
{
	gboolean allowed = FALSE;
	gboolean ret = FALSE;
	gboolean do_lock;
	gboolean nm_sleep;
	EggConsoleKit *console;
	GpmScreensaver *screensaver;
	guint32 throttle_cookie = 0;
#ifdef WITH_KEYRING
	gboolean lock_gnome_keyring;
	GnomeKeyringResult keyres;
#endif /* WITH_KEYRING */

	GError *dbus_error = NULL;
	GDBusProxy *proxy;
	GVariant *res = NULL;

	screensaver = gpm_screensaver_new ();

	if (!LOGIND_RUNNING()) {
		console = egg_console_kit_new ();
		egg_console_kit_can_suspend (console, &allowed, NULL);
		g_object_unref (console);

		if (!allowed) {
			egg_debug ("cannot suspend as not allowed from policy");
			g_set_error_literal (error, GPM_CONTROL_ERROR, GPM_CONTROL_ERROR_GENERAL, "Cannot suspend");
			goto out;
		}
	}

#ifdef WITH_KEYRING
	/* we should perhaps lock keyrings when sleeping #375681 */
	lock_gnome_keyring = g_settings_get_boolean (control->priv->settings, GPM_SETTINGS_LOCK_KEYRING_SUSPEND);
	if (lock_gnome_keyring) {
		keyres = gnome_keyring_lock_all_sync ();
		if (keyres != GNOME_KEYRING_RESULT_OK)
			egg_warning ("could not lock keyring");
	}
#endif /* WITH_KEYRING */

	do_lock = gpm_control_get_lock_policy (control, GPM_SETTINGS_LOCK_ON_SUSPEND);
	if (do_lock) {
		throttle_cookie = gpm_screensaver_add_throttle (screensaver, "suspend");
		gpm_screensaver_lock (screensaver);
	}

	nm_sleep = g_settings_get_boolean (control->priv->settings, GPM_SETTINGS_NETWORKMANAGER_SLEEP);
	if (nm_sleep)
		gpm_networkmanager_sleep ();

	/* Do the suspend */
	egg_debug ("emitting sleep");
	g_signal_emit (control, signals [SLEEP], 0, GPM_CONTROL_ACTION_SUSPEND);

	if (LOGIND_RUNNING()) {
		/* sleep via logind */
		proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
						       NULL,
						       "org.freedesktop.login1",
						       "/org/freedesktop/login1",
						       "org.freedesktop.login1.Manager",
						       NULL,
						       &dbus_error );
		if (proxy == NULL) {
			egg_error("Error connecting to dbus - %s", dbus_error->message);
			g_error_free (dbus_error);
			ret = FALSE;
			goto out;
		}
		res = g_dbus_proxy_call_sync (proxy, "Suspend",
					      g_variant_new( "(b)",FALSE),
					      G_DBUS_CALL_FLAGS_NONE,
					      -1,
					      NULL,
					      &dbus_error
					      );
		if (dbus_error != NULL ) {
			egg_debug ("Error in dbus - %s", dbus_error->message);
			g_error_free (dbus_error);
			ret = TRUE;
		}
		else {
			g_variant_unref(res);
			ret = TRUE;
		}
		g_object_unref(proxy);
	}
	else {
		console = egg_console_kit_new ();
		ret = egg_console_kit_suspend (console, error);
		g_object_unref (console);
	}

	egg_debug ("emitting resume");
	g_signal_emit (control, signals [RESUME], 0, GPM_CONTROL_ACTION_SUSPEND);

	if (do_lock) {
		gpm_screensaver_poke (screensaver);
		if (throttle_cookie)
			gpm_screensaver_remove_throttle (screensaver, throttle_cookie);
	}

	nm_sleep = g_settings_get_boolean (control->priv->settings, GPM_SETTINGS_NETWORKMANAGER_SLEEP);
	if (nm_sleep)
		gpm_networkmanager_wake ();

out:
	g_object_unref (screensaver);
	return ret;
}

/**
 * gpm_control_hibernate:
 **/
gboolean
gpm_control_hibernate (GpmControl *control, GError **error)
{
	gboolean allowed = FALSE;
	gboolean ret = FALSE;
	gboolean do_lock;
	gboolean nm_sleep;
	EggConsoleKit *console;
	GpmScreensaver *screensaver;
	guint32 throttle_cookie = 0;
#ifdef WITH_KEYRING
	gboolean lock_gnome_keyring;
	GnomeKeyringResult keyres;
#endif /* WITH_KEYRING */

	GError *dbus_error = NULL;
	GDBusProxy *proxy;
	GVariant *res = NULL;

	screensaver = gpm_screensaver_new ();

	if (!LOGIND_RUNNING()) {
		console = egg_console_kit_new ();
		egg_console_kit_can_hibernate (console, &allowed, NULL);
		g_object_unref (console);

		if (!allowed) {
			egg_debug ("cannot hibernate as not allowed from policy");
			g_set_error_literal (error, GPM_CONTROL_ERROR, GPM_CONTROL_ERROR_GENERAL, "Cannot hibernate");
			goto out;
		}
	}

#ifdef WITH_KEYRING
	/* we should perhaps lock keyrings when sleeping #375681 */
	lock_gnome_keyring = g_settings_get_boolean (control->priv->settings, GPM_SETTINGS_LOCK_KEYRING_HIBERNATE);
	if (lock_gnome_keyring) {
		keyres = gnome_keyring_lock_all_sync ();
		if (keyres != GNOME_KEYRING_RESULT_OK) {
			egg_warning ("could not lock keyring");
		}
	}
#endif /* WITH_KEYRING */

	do_lock = gpm_control_get_lock_policy (control, GPM_SETTINGS_LOCK_ON_HIBERNATE);
	if (do_lock) {
		throttle_cookie = gpm_screensaver_add_throttle (screensaver, "hibernate");
		gpm_screensaver_lock (screensaver);
	}

	nm_sleep = g_settings_get_boolean (control->priv->settings, GPM_SETTINGS_NETWORKMANAGER_SLEEP);
	if (nm_sleep)
		gpm_networkmanager_sleep ();

	egg_debug ("emitting sleep");
	g_signal_emit (control, signals [SLEEP], 0, GPM_CONTROL_ACTION_HIBERNATE);

	if (LOGIND_RUNNING()) {
		/* sleep via logind */
		proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
						       NULL,
						       "org.freedesktop.login1",
						       "/org/freedesktop/login1",
						       "org.freedesktop.login1.Manager",
						       NULL,
						       &dbus_error );
		if (proxy == NULL) {
			egg_error("Error connecting to dbus - %s", dbus_error->message);
			g_error_free (dbus_error);
			ret = FALSE;
			goto out;
		}
		res = g_dbus_proxy_call_sync (proxy, "Hibernate",
					      g_variant_new( "(b)",FALSE),
					      G_DBUS_CALL_FLAGS_NONE,
					      -1,
					      NULL,
					      &dbus_error
					      );
		if (dbus_error != NULL ) {
			egg_debug ("Error in dbus - %s", dbus_error->message);
			g_error_free (dbus_error);
			ret = TRUE;
		}
		else {
			g_variant_unref(res);
			ret = TRUE;
		}
	}
	else {
		console = egg_console_kit_new ();
		ret = egg_console_kit_hibernate (console, error);
		g_object_unref (console);
	}

	egg_debug ("emitting resume");
	g_signal_emit (control, signals [RESUME], 0, GPM_CONTROL_ACTION_HIBERNATE);

	if (do_lock) {
		gpm_screensaver_poke (screensaver);
		if (throttle_cookie)
			gpm_screensaver_remove_throttle (screensaver, throttle_cookie);
	}

	nm_sleep = g_settings_get_boolean (control->priv->settings, GPM_SETTINGS_NETWORKMANAGER_SLEEP);
	if (nm_sleep)
		gpm_networkmanager_wake ();

out:
	g_object_unref (screensaver);
	return ret;
}

/**
 * gpm_control_finalize:
 **/
static void
gpm_control_finalize (GObject *object)
{
	GpmControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_CONTROL (object));
	control = GPM_CONTROL (object);

	g_object_unref (control->priv->settings);

	g_return_if_fail (control->priv != NULL);
	G_OBJECT_CLASS (gpm_control_parent_class)->finalize (object);
}

/**
 * gpm_control_class_init:
 **/
static void
gpm_control_class_init (GpmControlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_control_finalize;

	signals [RESUME] =
		g_signal_new ("resume",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmControlClass, resume),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals [SLEEP] =
		g_signal_new ("sleep",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmControlClass, sleep),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	g_type_class_add_private (klass, sizeof (GpmControlPrivate));
}

/**
 * gpm_control_init:
 * @control: This control class instance
 **/
static void
gpm_control_init (GpmControl *control)
{
	control->priv = GPM_CONTROL_GET_PRIVATE (control);

	control->priv->settings = g_settings_new (GPM_SETTINGS_SCHEMA);
}

/**
 * gpm_control_new:
 * Return value: A new control class instance.
 **/
GpmControl *
gpm_control_new (void)
{
	if (gpm_control_object != NULL) {
		g_object_ref (gpm_control_object);
	} else {
		gpm_control_object = g_object_new (GPM_TYPE_CONTROL, NULL);
		g_object_add_weak_pointer (gpm_control_object, &gpm_control_object);
	}
	return GPM_CONTROL (gpm_control_object);
}

