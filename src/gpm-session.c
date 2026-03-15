/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2009 Richard Hughes <richard@hughsie.com>
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

#include "gpm-session.h"
#include "gpm-common.h"
#include "gpm-marshal.h"

static void     gpm_session_finalize   (GObject		*object);

#define GPM_SESSION_MANAGER_SERVICE			"org.gnome.SessionManager"
#define GPM_SESSION_MANAGER_PATH			"/org/gnome/SessionManager"
#define GPM_SESSION_MANAGER_INTERFACE			"org.gnome.SessionManager"
#define GPM_SESSION_MANAGER_PRESENCE_PATH		"/org/gnome/SessionManager/Presence"
#define GPM_SESSION_MANAGER_PRESENCE_INTERFACE		"org.gnome.SessionManager.Presence"
#define GPM_SESSION_MANAGER_CLIENT_PRIVATE_INTERFACE	"org.gnome.SessionManager.ClientPrivate"
#define GPM_DBUS_PROPERTIES_INTERFACE			"org.freedesktop.DBus.Properties"

typedef enum {
	GPM_SESSION_STATUS_ENUM_AVAILABLE = 0,
	GPM_SESSION_STATUS_ENUM_INVISIBLE,
	GPM_SESSION_STATUS_ENUM_BUSY,
	GPM_SESSION_STATUS_ENUM_IDLE,
	GPM_SESSION_STATUS_ENUM_UNKNOWN
} GpmSessionStatusEnum;

typedef enum {
	GPM_SESSION_INHIBIT_MASK_LOGOUT = 1,
	GPM_SESSION_INHIBIT_MASK_SWITCH = 2,
	GPM_SESSION_INHIBIT_MASK_SUSPEND = 4,
	GPM_SESSION_INHIBIT_MASK_IDLE = 8
} GpmSessionInhibitMask;

struct GpmSessionPrivate
{
	GDBusProxy		*proxy;
	GDBusProxy		*proxy_presence;
	GDBusProxy		*proxy_client_private;
	GDBusProxy		*proxy_prop;
	gboolean		 is_idle_old;
	gboolean		 is_idle_inhibited_old;
	gboolean		 is_suspend_inhibited_old;
};

enum {
	IDLE_CHANGED,
	INHIBITED_CHANGED,
	STOP,
	QUERY_END_SESSION,
	END_SESSION,
	CANCEL_END_SESSION,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_session_object = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (GpmSession, gpm_session, G_TYPE_OBJECT)

/**
 * gpm_session_logout:
 **/
gboolean
gpm_session_logout (GpmSession *session)
{
	g_return_val_if_fail (GPM_IS_SESSION (session), FALSE);

	/* no mate-session */
	if (session->priv->proxy == NULL) {
		g_warning ("no mate-session");
		return FALSE;
	}

	/* we have to use no reply, as the SM calls into g-p-m to get the can_suspend property */
	g_dbus_proxy_call (session->priv->proxy,
			   "Shutdown",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL,
			   NULL,
			   NULL);
	return TRUE;
}

/**
 * gpm_session_get_idle:
 **/
gboolean
gpm_session_get_idle (GpmSession *session)
{
	g_return_val_if_fail (GPM_IS_SESSION (session), FALSE);
	return session->priv->is_idle_old;
}

/**
 * gpm_session_get_idle_inhibited:
 **/
gboolean
gpm_session_get_idle_inhibited (GpmSession *session)
{
	g_return_val_if_fail (GPM_IS_SESSION (session), FALSE);
	return session->priv->is_idle_inhibited_old;
}

/**
 * gpm_session_get_suspend_inhibited:
 **/
gboolean
gpm_session_get_suspend_inhibited (GpmSession *session)
{
	g_return_val_if_fail (GPM_IS_SESSION (session), FALSE);
	return session->priv->is_suspend_inhibited_old;
}

/**
 * gpm_session_presence_status_changed_cb:
 **/
static void
gpm_session_presence_status_changed_cb (GDBusProxy *proxy,
					gchar *sender_name,
					gchar *signal_name,
					GVariant *parameters,
					GpmSession *session)
{
	guint status;
	gboolean is_idle;

	if (g_strcmp0 (signal_name, "StatusChanged") != 0)
		return;

	g_variant_get (parameters, "(u)", &status);

	is_idle = (status == GPM_SESSION_STATUS_ENUM_IDLE);
	if (is_idle != session->priv->is_idle_old) {
		g_debug ("emitting idle-changed : (%i)", is_idle);
		session->priv->is_idle_old = is_idle;
		g_signal_emit (session, signals [IDLE_CHANGED], 0, is_idle);
	}
}

/**
 * gpm_session_is_idle:
 **/
static gboolean
gpm_session_is_idle (GpmSession *session)
{
	GVariant *result;
	GVariant *status_variant;
	guint status;
	gboolean ret;
	gboolean is_idle = FALSE;
	GError *error = NULL;

	/* no mate-session */
	if (session->priv->proxy_prop == NULL) {
		g_warning ("no mate-session");
		goto out;
	}

	/* find out if this change altered the inhibited state */
	result = g_dbus_proxy_call_sync (session->priv->proxy_prop,
					 "Get",
					 g_variant_new ("(ss)", GPM_SESSION_MANAGER_PRESENCE_INTERFACE, "status"),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	ret = (result != NULL);
	if (!ret) {
		g_warning ("failed to get idle status: %s", error->message);
		g_error_free (error);
		is_idle = FALSE;
		goto out;
	}
	g_variant_get (result, "(v)", &status_variant);
	status = g_variant_get_uint32 (status_variant);
	g_variant_unref (status_variant);
	g_variant_unref (result);
	is_idle = (status == GPM_SESSION_STATUS_ENUM_IDLE);
out:
	return is_idle;
}

/**
 * gpm_session_is_idle_inhibited:
 **/
static gboolean
gpm_session_is_idle_inhibited (GpmSession *session)
{
	GVariant *result;
	gboolean ret;
	gboolean is_inhibited = FALSE;
	GError *error = NULL;

	/* no mate-session */
	if (session->priv->proxy == NULL) {
		g_warning ("no mate-session");
		goto out;
	}

	/* find out if this change altered the inhibited state */
	result = g_dbus_proxy_call_sync (session->priv->proxy,
					 "IsInhibited",
					 g_variant_new ("(u)", GPM_SESSION_INHIBIT_MASK_IDLE),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	ret = (result != NULL);
	if (result != NULL) {
		g_variant_get (result, "(b)", &is_inhibited);
		g_variant_unref (result);
	}
	if (!ret) {
		g_warning ("failed to get inhibit status: %s", error->message);
		g_error_free (error);
		is_inhibited = FALSE;
	}
out:
	return is_inhibited;
}

/**
 * gpm_session_is_suspend_inhibited:
 **/
static gboolean
gpm_session_is_suspend_inhibited (GpmSession *session)
{
	GVariant *result;
	gboolean ret;
	gboolean is_inhibited = FALSE;
	GError *error = NULL;

	/* no mate-session */
	if (session->priv->proxy == NULL) {
		g_warning ("no mate-session");
		goto out;
	}

	/* find out if this change altered the inhibited state */
	result = g_dbus_proxy_call_sync (session->priv->proxy,
					 "IsInhibited",
					 g_variant_new ("(u)", GPM_SESSION_INHIBIT_MASK_SUSPEND),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	ret = (result != NULL);
	if (result != NULL) {
		g_variant_get (result, "(b)", &is_inhibited);
		g_variant_unref (result);
	}
	if (!ret) {
		g_warning ("failed to get inhibit status: %s", error->message);
		g_error_free (error);
		is_inhibited = FALSE;
	}
out:
	return is_inhibited;
}

/**
 * gpm_session_stop_cb:
 **/
static void
gpm_session_stop_cb (GpmSession *session)
{
	g_debug ("emitting ::stop()");
	g_signal_emit (session, signals [STOP], 0);
}

/**
 * gpm_session_query_end_session_cb:
 **/
static void
gpm_session_query_end_session_cb (GpmSession *session, guint flags)
{
	g_debug ("emitting ::query-end-session(%u)", flags);
	g_signal_emit (session, signals [QUERY_END_SESSION], 0, flags);
}

/**
 * gpm_session_end_session_cb:
 **/
static void
gpm_session_end_session_cb (GpmSession *session, guint flags)
{
	g_debug ("emitting ::end-session(%u)", flags);
	g_signal_emit (session, signals [END_SESSION], 0, flags);
}

static void
gpm_session_client_private_signal_cb (GDBusProxy *proxy,
				      gchar *sender_name,
				      gchar *signal_name,
				      GVariant *parameters,
				      GpmSession *session)
{
	guint flags;

	if (g_strcmp0 (signal_name, "Stop") == 0) {
		gpm_session_stop_cb (session);
		return;
	}

	if (g_strcmp0 (signal_name, "QueryEndSession") == 0) {
		g_variant_get (parameters, "(u)", &flags);
		gpm_session_query_end_session_cb (session, flags);
		return;
	}

	if (g_strcmp0 (signal_name, "EndSession") == 0) {
		g_variant_get (parameters, "(u)", &flags);
		gpm_session_end_session_cb (session, flags);
	}
}

/**
 * gpm_session_end_session_response:
 **/
gboolean
gpm_session_end_session_response (GpmSession *session, gboolean is_okay, const gchar *reason)
{
	GVariant *result;
	gboolean ret = FALSE;
	GError *error = NULL;

	g_return_val_if_fail (GPM_IS_SESSION (session), FALSE);
	g_return_val_if_fail (session->priv->proxy_client_private != NULL, FALSE);

	/* no mate-session */
	if (session->priv->proxy_client_private == NULL) {
		g_warning ("no mate-session proxy");
		goto out;
	}

	/* send response */
	result = g_dbus_proxy_call_sync (session->priv->proxy_client_private,
					 "EndSessionResponse",
					 g_variant_new ("(bs)", is_okay, reason),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	ret = (result != NULL);
	if (result != NULL)
		g_variant_unref (result);
	if (!ret) {
		g_warning ("failed to send session response: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * gpm_session_register_client:
 **/
gboolean
gpm_session_register_client (GpmSession *session, const gchar *app_id, const gchar *client_startup_id)
{
	GVariant *result;
	const gchar *client_id_tmp;
	gboolean ret = FALSE;
	gchar *client_id = NULL;
	GError *error = NULL;

	g_return_val_if_fail (GPM_IS_SESSION (session), FALSE);

	/* no mate-session */
	if (session->priv->proxy == NULL) {
		g_warning ("no mate-session");
		goto out;
	}

	/* find out if this change altered the inhibited state */
	result = g_dbus_proxy_call_sync (session->priv->proxy,
					 "RegisterClient",
					 g_variant_new ("(ss)", app_id, client_startup_id),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	ret = (result != NULL);
	if (!ret) {
		g_warning ("failed to register client '%s': %s", client_startup_id, error->message);
		g_error_free (error);
		goto out;
	}
	g_variant_get (result, "(&o)", &client_id_tmp);
	client_id = g_strdup (client_id_tmp);
	g_variant_unref (result);

	/* get org.gnome.SessionManager.ClientPrivate interface */
	session->priv->proxy_client_private = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
							     G_DBUS_PROXY_FLAGS_NONE,
							     NULL,
							     GPM_SESSION_MANAGER_SERVICE,
							     client_id,
							     GPM_SESSION_MANAGER_CLIENT_PRIVATE_INTERFACE,
							     NULL,
							     &error);
	if (session->priv->proxy_client_private == NULL) {
		g_warning ("DBUS error: %s", error->message);
		g_error_free (error);
		goto out;
	}

	g_signal_connect (session->priv->proxy_client_private, "g-signal",
			  G_CALLBACK (gpm_session_client_private_signal_cb), session);

	g_debug ("registered startup '%s' to client id '%s'", client_startup_id, client_id);
out:
	g_free (client_id);
	return ret;
}

/**
 * gpm_session_inhibit_changed_cb:
 **/
static void
gpm_session_inhibit_changed_cb (GDBusProxy *proxy,
				gchar *sender_name,
				gchar *signal_name,
				GVariant *parameters,
				GpmSession *session)
{
	gboolean is_idle_inhibited;
	gboolean is_suspend_inhibited;

	if (g_strcmp0 (signal_name, "InhibitorAdded") != 0 &&
	    g_strcmp0 (signal_name, "InhibitorRemoved") != 0)
		return;

	is_idle_inhibited = gpm_session_is_idle_inhibited (session);
	is_suspend_inhibited = gpm_session_is_suspend_inhibited (session);
	if (is_idle_inhibited != session->priv->is_idle_inhibited_old || is_suspend_inhibited != session->priv->is_suspend_inhibited_old) {
		g_debug ("emitting inhibited-changed : idle=(%i), suspend=(%i)", is_idle_inhibited, is_suspend_inhibited);
		session->priv->is_idle_inhibited_old = is_idle_inhibited;
		session->priv->is_suspend_inhibited_old = is_suspend_inhibited;
		g_signal_emit (session, signals [INHIBITED_CHANGED], 0, is_idle_inhibited, is_suspend_inhibited);
	}
}

/**
 * gpm_session_class_init:
 * @klass: This class instance
 **/
static void
gpm_session_class_init (GpmSessionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_session_finalize;

	signals [IDLE_CHANGED] =
		g_signal_new ("idle-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmSessionClass, idle_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	signals [INHIBITED_CHANGED] =
		g_signal_new ("inhibited-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmSessionClass, inhibited_changed),
			      NULL, NULL, gpm_marshal_VOID__BOOLEAN_BOOLEAN,
			      G_TYPE_NONE, 2, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	signals [STOP] =
		g_signal_new ("stop",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmSessionClass, stop),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals [QUERY_END_SESSION] =
		g_signal_new ("query-end-session",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmSessionClass, query_end_session),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [END_SESSION] =
		g_signal_new ("end-session",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmSessionClass, end_session),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals [CANCEL_END_SESSION] =
		g_signal_new ("cancel-end-session",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmSessionClass, cancel_end_session),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/**
 * gpm_session_init:
 * @session: This class instance
 **/
static void
gpm_session_init (GpmSession *session)
{
	GError *error = NULL;

	session->priv = gpm_session_get_instance_private (session);
	session->priv->is_idle_old = FALSE;
	session->priv->is_idle_inhibited_old = FALSE;
	session->priv->is_suspend_inhibited_old = FALSE;
	session->priv->proxy = NULL;
	session->priv->proxy_presence = NULL;
	session->priv->proxy_client_private = NULL;
	session->priv->proxy_prop = NULL;

	/* get org.gnome.SessionManager interface */
	session->priv->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
						      G_DBUS_PROXY_FLAGS_NONE,
						      NULL,
						      GPM_SESSION_MANAGER_SERVICE,
						      GPM_SESSION_MANAGER_PATH,
						      GPM_SESSION_MANAGER_INTERFACE,
						      NULL,
						      &error);
	if (session->priv->proxy == NULL) {
		g_warning ("DBUS error: %s", error->message);
		g_error_free (error);
		return;
	}

	/* get org.gnome.SessionManager.Presence interface */
	g_clear_error (&error);
	session->priv->proxy_presence = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
							       G_DBUS_PROXY_FLAGS_NONE,
							       NULL,
							       GPM_SESSION_MANAGER_SERVICE,
							       GPM_SESSION_MANAGER_PRESENCE_PATH,
							       GPM_SESSION_MANAGER_PRESENCE_INTERFACE,
							       NULL,
							       &error);
	if (session->priv->proxy_presence == NULL) {
		g_warning ("DBUS error: %s", error->message);
		g_error_free (error);
		return;
	}

	/* get properties interface */
	g_clear_error (&error);
	session->priv->proxy_prop = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
						       G_DBUS_PROXY_FLAGS_NONE,
						       NULL,
						       GPM_SESSION_MANAGER_SERVICE,
						       GPM_SESSION_MANAGER_PRESENCE_PATH,
						       GPM_DBUS_PROPERTIES_INTERFACE,
						       NULL,
						       &error);
	if (session->priv->proxy_prop == NULL) {
		g_warning ("DBUS error: %s", error->message);
		g_error_free (error);
		return;
	}

	g_signal_connect (session->priv->proxy_presence, "g-signal",
			  G_CALLBACK (gpm_session_presence_status_changed_cb), session);

	g_signal_connect (session->priv->proxy, "g-signal",
			  G_CALLBACK (gpm_session_inhibit_changed_cb), session);

	/* coldplug */
	session->priv->is_idle_inhibited_old = gpm_session_is_idle_inhibited (session);
	session->priv->is_suspend_inhibited_old = gpm_session_is_suspend_inhibited (session);
	session->priv->is_idle_old = gpm_session_is_idle (session);
	g_debug ("idle: %i, idle_inhibited: %i, suspend_inhibited: %i", session->priv->is_idle_old, session->priv->is_idle_inhibited_old, session->priv->is_suspend_inhibited_old);
}

/**
 * gpm_session_finalize:
 * @object: This class instance
 **/
static void
gpm_session_finalize (GObject *object)
{
	GpmSession *session;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_SESSION (object));

	session = GPM_SESSION (object);
	session->priv = gpm_session_get_instance_private (session);

	if (session->priv->proxy != NULL)
		g_object_unref (session->priv->proxy);
	if (session->priv->proxy_presence != NULL)
		g_object_unref (session->priv->proxy_presence);
	if (session->priv->proxy_client_private != NULL)
		g_object_unref (session->priv->proxy_client_private);
	if (session->priv->proxy_prop != NULL)
		g_object_unref (session->priv->proxy_prop);

	G_OBJECT_CLASS (gpm_session_parent_class)->finalize (object);
}

/**
 * gpm_session_new:
 * Return value: new GpmSession instance.
 **/
GpmSession *
gpm_session_new (void)
{
	if (gpm_session_object != NULL) {
		g_object_ref (gpm_session_object);
	} else {
		gpm_session_object = g_object_new (GPM_TYPE_SESSION, NULL);
		g_object_add_weak_pointer (gpm_session_object, &gpm_session_object);
	}
	return GPM_SESSION (gpm_session_object);
}
