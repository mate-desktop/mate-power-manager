/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2008 Richard Hughes <richard@hughsie.com>
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
#include <unistd.h>
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>

#include "egg-console-kit.h"

static void     egg_console_kit_finalize	(GObject		*object);

#define CONSOLEKIT_NAME			"org.freedesktop.ConsoleKit"
#define CONSOLEKIT_PATH			"/org/freedesktop/ConsoleKit"
#define CONSOLEKIT_INTERFACE		"org.freedesktop.ConsoleKit"

#define CONSOLEKIT_MANAGER_PATH	 	"/org/freedesktop/ConsoleKit/Manager"
#define CONSOLEKIT_MANAGER_INTERFACE    "org.freedesktop.ConsoleKit.Manager"
#define CONSOLEKIT_SEAT_INTERFACE       "org.freedesktop.ConsoleKit.Seat"
#define CONSOLEKIT_SESSION_INTERFACE    "org.freedesktop.ConsoleKit.Session"

struct EggConsoleKitPrivate
{
	GDBusConnection		*connection;
	GDBusProxy		*proxy_manager;
	GDBusProxy		*proxy_session;
	gchar			*session_id;
};

enum {
	EGG_CONSOLE_KIT_ACTIVE_CHANGED,
	EGG_CONSOLE_KIT_LAST_SIGNAL
};

static gpointer egg_console_kit_object = NULL;
static guint signals [EGG_CONSOLE_KIT_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (EggConsoleKit, egg_console_kit, G_TYPE_OBJECT)

/**
 * egg_console_kit_restart:
 **/
gboolean
egg_console_kit_restart (EggConsoleKit *console, GError **error)
{
	GVariant *result;
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (EGG_IS_CONSOLE_KIT (console), FALSE);
	g_return_val_if_fail (console->priv->proxy_manager != NULL, FALSE);

	result = g_dbus_proxy_call_sync (console->priv->proxy_manager,
					 "Restart",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error_local);
	ret = (result != NULL);
	if (result != NULL)
		g_variant_unref (result);
	if (!ret) {
		g_warning ("Couldn't restart: %s", error_local->message);
		if (error != NULL)
			*error = g_error_new (1, 0, "%s", error_local->message);
		g_error_free (error_local);
	}
	return ret;
}

/**
 * egg_console_kit_stop:
 **/
gboolean
egg_console_kit_stop (EggConsoleKit *console, GError **error)
{
	GVariant *result;
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (EGG_IS_CONSOLE_KIT (console), FALSE);
	g_return_val_if_fail (console->priv->proxy_manager != NULL, FALSE);

	result = g_dbus_proxy_call_sync (console->priv->proxy_manager,
					 "Stop",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error_local);
	ret = (result != NULL);
	if (result != NULL)
		g_variant_unref (result);
	if (!ret) {
		g_warning ("Couldn't stop: %s", error_local->message);
		if (error != NULL)
			*error = g_error_new (1, 0, "%s", error_local->message);
		g_error_free (error_local);
	}
	return ret;
}

/**
 * egg_console_kit_suspend:
 **/
gboolean
egg_console_kit_suspend (EggConsoleKit *console, GError **error)
{
	GVariant *result;
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (EGG_IS_CONSOLE_KIT (console), FALSE);
	g_return_val_if_fail (console->priv->proxy_manager != NULL, FALSE);

	result = g_dbus_proxy_call_sync (console->priv->proxy_manager,
					 "Suspend",
					 g_variant_new ("(b)", TRUE),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error_local);
	ret = (result != NULL);
	if (result != NULL)
		g_variant_unref (result);
	if (!ret) {
		g_warning ("Couldn't suspend: %s", error_local->message);
		if (error != NULL)
			*error = g_error_new (1, 0, "%s", error_local->message);
		g_error_free (error_local);
	}
	return ret;
}

/**
 * egg_console_kit_hibernate:
 **/
gboolean
egg_console_kit_hibernate (EggConsoleKit *console, GError **error)
{
	GVariant *result;
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (EGG_IS_CONSOLE_KIT (console), FALSE);
	g_return_val_if_fail (console->priv->proxy_manager != NULL, FALSE);

	result = g_dbus_proxy_call_sync (console->priv->proxy_manager,
					 "Hibernate",
					 g_variant_new ("(b)", TRUE),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error_local);
	ret = (result != NULL);
	if (result != NULL)
		g_variant_unref (result);
	if (!ret) {
		g_warning ("Couldn't hibernate: %s", error_local->message);
		if (error != NULL)
			*error = g_error_new (1, 0, "%s", error_local->message);
		g_error_free (error_local);
	}
	return ret;
}

/**
 * egg_console_kit_can_stop:
 **/
gboolean
egg_console_kit_can_stop (EggConsoleKit *console, gboolean *can_stop, GError **error)
{
	GVariant *result;
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (EGG_IS_CONSOLE_KIT (console), FALSE);
	g_return_val_if_fail (console->priv->proxy_manager != NULL, FALSE);

	result = g_dbus_proxy_call_sync (console->priv->proxy_manager,
					 "CanStop",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error_local);
	ret = (result != NULL);
	if (result != NULL) {
		g_variant_get (result, "(b)", can_stop);
		g_variant_unref (result);
	}
	if (!ret) {
		g_warning ("Couldn't do CanStop: %s", error_local->message);
		if (error != NULL)
			*error = g_error_new (1, 0, "%s", error_local->message);
		g_error_free (error_local);
		/* CanStop was only added in new versions of ConsoleKit,
		 * so assume true if this failed */
		*can_stop = TRUE;
	}
	return ret;
}

/**
 * egg_console_kit_can_restart:
 **/
gboolean
egg_console_kit_can_restart (EggConsoleKit *console, gboolean *can_restart, GError **error)
{
	GVariant *result;
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (EGG_IS_CONSOLE_KIT (console), FALSE);
	g_return_val_if_fail (console->priv->proxy_manager != NULL, FALSE);

	result = g_dbus_proxy_call_sync (console->priv->proxy_manager,
					 "CanRestart",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error_local);
	ret = (result != NULL);
	if (result != NULL) {
		g_variant_get (result, "(b)", can_restart);
		g_variant_unref (result);
	}
	if (!ret) {
		g_warning ("Couldn't do CanRestart: %s", error_local->message);
		if (error != NULL)
			*error = g_error_new (1, 0, "%s", error_local->message);
		g_error_free (error_local);
		/* CanRestart was only added in new versions of ConsoleKit,
		 * so assume true if this failed */
		*can_restart = TRUE;
	}
	return ret;
}

/**
 * egg_console_kit_can_suspend:
 **/
gboolean
egg_console_kit_can_suspend (EggConsoleKit *console, gboolean *can_suspend, GError **error)
{
	GError *error_local = NULL;
	GVariant *result;
	gboolean ret;
	const gchar *retval;

	g_return_val_if_fail (EGG_IS_CONSOLE_KIT (console), FALSE);
	g_return_val_if_fail (console->priv->proxy_manager != NULL, FALSE);

	result = g_dbus_proxy_call_sync (console->priv->proxy_manager,
					 "CanSuspend",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error_local);
	ret = (result != NULL);
	if (result != NULL)
		g_variant_get (result, "(&s)", &retval);
	if (!ret) {
		g_warning ("Couldn't do CanSuspend: %s", error_local->message);
		if (error != NULL)
			*error = g_error_new (1, 0, "%s", error_local->message);
		g_error_free (error_local);
		retval = "";
	}

	*can_suspend = g_strcmp0 (retval, "yes") == 0 ||
		       g_strcmp0 (retval, "challenge") == 0;
	if (result != NULL)
		g_variant_unref (result);
	return ret;
}

/**
 * egg_console_kit_can_hibernate:
 **/

gboolean
egg_console_kit_can_hibernate (EggConsoleKit *console, gboolean *can_hibernate, GError **error)
{
	GError *error_local = NULL;
	GVariant *result;
	gboolean ret;
	const gchar *retval;

	g_return_val_if_fail (EGG_IS_CONSOLE_KIT (console), FALSE);
	g_return_val_if_fail (console->priv->proxy_manager != NULL, FALSE);

	result = g_dbus_proxy_call_sync (console->priv->proxy_manager,
					 "CanHibernate",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error_local);
	ret = (result != NULL);
	if (result != NULL)
		g_variant_get (result, "(&s)", &retval);
	if (!ret) {
		g_warning ("Couldn't do CanHibernate: %s", error_local->message);
		if (error != NULL)
			*error = g_error_new (1, 0, "%s", error_local->message);
		g_error_free (error_local);
		retval = "";
	}

	*can_hibernate = g_strcmp0 (retval, "yes") == 0 ||
	                 g_strcmp0 (retval, "challenge") == 0;
	if (result != NULL)
		g_variant_unref (result);
	return ret;
}

/**
 * egg_console_kit_is_local:
 *
 * Return value: Returns whether the session is local
 **/
gboolean
egg_console_kit_is_local (EggConsoleKit *console, gboolean *is_local, GError **error)
{
	GVariant *result = NULL;
	gboolean ret = FALSE;
	GError *error_local = NULL;

	g_return_val_if_fail (EGG_IS_CONSOLE_KIT (console), FALSE);
	g_return_val_if_fail (is_local != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* maybe console kit does not know about our session */
	if (console->priv->proxy_session == NULL) {
		g_warning ("no ConsoleKit session");
		goto out;
	}

	/* is our session local */
	result = g_dbus_proxy_call_sync (console->priv->proxy_session,
					 "IsLocal",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error_local);
	if (result == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "IsLocal failed: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}
	g_variant_get (result, "(b)", is_local);

	/* success */
	ret = TRUE;
out:
	if (result != NULL)
		g_variant_unref (result);
	return ret;
}

/**
 * egg_console_kit_is_active:
 *
 * Return value: Returns whether the session is active on the Seat that it is attached to.
 **/
gboolean
egg_console_kit_is_active (EggConsoleKit *console, gboolean *is_active, GError **error)
{
	GVariant *result = NULL;
	gboolean ret = FALSE;
	GError *error_local = NULL;

	g_return_val_if_fail (EGG_IS_CONSOLE_KIT (console), FALSE);
	g_return_val_if_fail (is_active != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* maybe console kit does not know about our session */
	if (console->priv->proxy_session == NULL) {
		g_warning ("no ConsoleKit session");
		goto out;
	}

	/* is our session active */
	result = g_dbus_proxy_call_sync (console->priv->proxy_session,
					 "IsActive",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error_local);
	if (result == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "IsActive failed: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}
	g_variant_get (result, "(b)", is_active);

	/* success */
	ret = TRUE;
out:
	if (result != NULL)
		g_variant_unref (result);
	return ret;
}

/**
 * egg_console_kit_active_changed_cb:
 **/
static void
egg_console_kit_active_changed_cb (GDBusProxy *proxy,
				   gchar *sender_name,
				   gchar *signal_name,
				   GVariant *parameters,
				   EggConsoleKit *console)
{
	gboolean active;

	if (g_strcmp0 (signal_name, "ActiveChanged") != 0)
		return;

	g_variant_get (parameters, "(b)", &active);
	g_debug ("emitting active: %i", active);
	g_signal_emit (console, signals [EGG_CONSOLE_KIT_ACTIVE_CHANGED], 0, active);
}

/**
 * egg_console_kit_class_init:
 * @klass: The EggConsoleKitClass
 **/
static void
egg_console_kit_class_init (EggConsoleKitClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = egg_console_kit_finalize;

	signals [EGG_CONSOLE_KIT_ACTIVE_CHANGED] =
		g_signal_new ("active-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EggConsoleKitClass, active_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

/**
 * egg_console_kit_init:
 **/
static void
egg_console_kit_init (EggConsoleKit *console)
{
	GVariant *result;
	gboolean ret;
	GError *error = NULL;
	guint32 pid;
	const gchar *session_id;

	console->priv = egg_console_kit_get_instance_private (console);
	console->priv->connection = NULL;
	console->priv->proxy_manager = NULL;
	console->priv->proxy_session = NULL;
	console->priv->session_id = NULL;

	/* connect to D-Bus */
	console->priv->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
	if (console->priv->connection == NULL) {
		g_warning ("Failed to connect to the D-Bus daemon: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* connect to ConsoleKit */
	console->priv->proxy_manager = g_dbus_proxy_new_sync (console->priv->connection,
						      G_DBUS_PROXY_FLAGS_NONE,
						      NULL,
						      CONSOLEKIT_NAME,
						      CONSOLEKIT_MANAGER_PATH,
						      CONSOLEKIT_MANAGER_INTERFACE,
						      NULL,
						      &error);
	if (console->priv->proxy_manager == NULL) {
		g_warning ("cannot connect to ConsoleKit: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get the session we are running in */
	pid = getpid ();
	result = g_dbus_proxy_call_sync (console->priv->proxy_manager,
					 "GetSessionForUnixProcess",
					 g_variant_new ("(u)", pid),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	ret = (result != NULL);
	if (!ret) {
		g_warning ("Failed to get session for pid %u: %s", pid, error->message);
		g_error_free (error);
		goto out;
	}
	g_variant_get (result, "(&o)", &session_id);
	console->priv->session_id = g_strdup (session_id);
	g_variant_unref (result);
	g_debug ("ConsoleKit session ID: %s", console->priv->session_id);

	/* connect to session */
	console->priv->proxy_session = g_dbus_proxy_new_sync (console->priv->connection,
						      G_DBUS_PROXY_FLAGS_NONE,
						      NULL,
						      CONSOLEKIT_NAME,
						      console->priv->session_id,
						      CONSOLEKIT_SESSION_INTERFACE,
						      NULL,
						      &error);
	if (console->priv->proxy_session == NULL) {
		g_warning ("cannot connect to: %s: %s", console->priv->session_id, error->message);
		g_error_free (error);
		goto out;
	}
	g_signal_connect (console->priv->proxy_session, "g-signal",
			  G_CALLBACK (egg_console_kit_active_changed_cb), console);

out:
	return;
}

/**
 * egg_console_kit_finalize:
 * @object: The object to finalize
 **/
static void
egg_console_kit_finalize (GObject *object)
{
	EggConsoleKit *console;

	g_return_if_fail (EGG_IS_CONSOLE_KIT (object));

	console = EGG_CONSOLE_KIT (object);

	g_return_if_fail (console->priv != NULL);
	if (console->priv->proxy_manager != NULL)
		g_object_unref (console->priv->proxy_manager);
	if (console->priv->proxy_session != NULL)
		g_object_unref (console->priv->proxy_session);
	if (console->priv->connection != NULL)
		g_object_unref (console->priv->connection);
	g_free (console->priv->session_id);

	G_OBJECT_CLASS (egg_console_kit_parent_class)->finalize (object);
}

/**
 * egg_console_kit_new:
 *
 * Return value: a new EggConsoleKit object.
 **/
EggConsoleKit *
egg_console_kit_new (void)
{
	if (egg_console_kit_object != NULL) {
		g_object_ref (egg_console_kit_object);
	} else {
		egg_console_kit_object = g_object_new (EGG_TYPE_CONSOLE_KIT, NULL);
		g_object_add_weak_pointer (egg_console_kit_object, &egg_console_kit_object);
	}

	return EGG_CONSOLE_KIT (egg_console_kit_object);
}

