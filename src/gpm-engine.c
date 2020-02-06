/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2008 Richard Hughes <richard@hughsie.com>
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
#include <glib/gi18n.h>
#include <libupower-glib/upower.h>

#include "egg-debug.h"

#include "gpm-common.h"
#include "gpm-upower.h"
#include "gpm-marshal.h"
#include "gpm-engine.h"
#include "gpm-icon-names.h"
#include "gpm-phone.h"

static void     gpm_engine_finalize   (GObject	  *object);

#define GPM_ENGINE_RESUME_DELAY		2*1000
#define GPM_ENGINE_WARN_ACCURACY	20

struct GpmEnginePrivate
{
	GSettings		*settings;
	UpClient		*client;
	UpDevice		*battery_composite;
	GPtrArray		*array;
	GpmPhone		*phone;
	GpmIconPolicy		 icon_policy;
	gchar			*previous_icon;
	gchar			*previous_summary;

	gboolean		 use_time_primary;
	gboolean		 time_is_accurate;

	guint			 low_percentage;
	guint			 critical_percentage;
	guint			 action_percentage;
	guint			 low_time;
	guint			 critical_time;
	guint			 action_time;
};

enum {
	ICON_CHANGED,
	SUMMARY_CHANGED,
	FULLY_CHARGED,
	CHARGE_LOW,
	CHARGE_CRITICAL,
	CHARGE_ACTION,
	DISCHARGING,
	LOW_CAPACITY,
	DEVICES_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_engine_object = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (GpmEngine, gpm_engine, G_TYPE_OBJECT)

static UpDevice *gpm_engine_get_composite_device (GpmEngine *engine, UpDevice *original_device);
static UpDevice *gpm_engine_update_composite_device (GpmEngine *engine, UpDevice *original_device);
static void gpm_engine_device_changed_cb (UpDevice *device, GParamSpec *pspec, GpmEngine *engine);

#define GPM_ENGINE_WARNING_NONE UP_DEVICE_LEVEL_NONE
#define GPM_ENGINE_WARNING_DISCHARGING UP_DEVICE_LEVEL_DISCHARGING
#define GPM_ENGINE_WARNING_LOW UP_DEVICE_LEVEL_LOW
#define GPM_ENGINE_WARNING_CRITICAL UP_DEVICE_LEVEL_CRITICAL
#define GPM_ENGINE_WARNING_ACTION UP_DEVICE_LEVEL_ACTION

/**
 * gpm_engine_get_warning:
 *
 * This gets the possible engine state for the device according to the
 * policy, which could be per-percent, or per-time.
 *
 * Return value: A GpmEngine state, e.g. GPM_ENGINE_WARNING_DISCHARGING
 **/
static UpDeviceLevel
gpm_engine_get_warning (GpmEngine *engine, UpDevice *device)
{
	UpDeviceLevel warning;
	g_object_get (device, "warning-level", &warning, NULL);
	return warning;
}

/**
 * gpm_engine_get_summary:
 * @engine: This engine class instance
 * @string: The returned string
 *
 * Returns the complete tooltip ready for display
 **/
gchar *
gpm_engine_get_summary (GpmEngine *engine)
{
	guint i;
	GPtrArray *array;
	UpDevice *device;
	UpDeviceState state;
	GString *tooltip = NULL;
	gchar *part;
	gboolean is_present;

	g_return_val_if_fail (GPM_IS_ENGINE (engine), NULL);

	/* need to get AC state */
	tooltip = g_string_new ("");

	/* do we have specific device types? */
	array = engine->priv->array;
	for (i=0;i<array->len;i++) {
		device = g_ptr_array_index (engine->priv->array, i);
		g_object_get (device,
			      "is-present", &is_present,
			      "state", &state,
			      NULL);
		if (!is_present)
			continue;
		if (state == UP_DEVICE_STATE_EMPTY)
			continue;
		part = gpm_upower_get_device_summary (device);
		if (part != NULL)
			g_string_append_printf (tooltip, "%s\n", part);
		g_free (part);
	}

	/* remove the last \n */
	g_string_truncate (tooltip, tooltip->len-1);

	egg_debug ("tooltip: %s", tooltip->str);

	return g_string_free (tooltip, FALSE);
}

/**
 * gpm_engine_get_icon_priv:
 *
 * Returns the icon
 **/
static gchar *
gpm_engine_get_icon_priv (GpmEngine *engine, UpDeviceKind device_kind, UpDeviceLevel warning, gboolean use_state)
{
	guint i;
	GPtrArray *array;
	UpDevice *device;
	UpDeviceLevel warning_temp;
	UpDeviceKind kind;
	UpDeviceState state;
	gboolean is_present;

	/* do we have specific device types? */
	array = engine->priv->array;
	for (i=0;i<array->len;i++) {
		device = g_ptr_array_index (engine->priv->array, i);

		/* get device properties */
		g_object_get (device,
			      "kind", &kind,
			      "state", &state,
			      "is-present", &is_present,
			      NULL);

		/* if battery then use composite device to cope with multiple batteries */
		if (kind == UP_DEVICE_KIND_BATTERY)
			device = gpm_engine_get_composite_device (engine, device);

		warning_temp = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(device), "engine-warning-old"));
		if (kind == device_kind && is_present) {
			if (warning != GPM_ENGINE_WARNING_NONE) {
				if (warning_temp == warning)
					return gpm_upower_get_device_icon (device);
				continue;
			}
			if (use_state) {
				if (state == UP_DEVICE_STATE_CHARGING || state == UP_DEVICE_STATE_DISCHARGING)
					return gpm_upower_get_device_icon (device);
				continue;
			}
			return gpm_upower_get_device_icon (device);
		}
	}
	return NULL;
}

/**
 * gpm_engine_get_icon:
 *
 * Returns the icon
 **/
gchar *
gpm_engine_get_icon (GpmEngine *engine)
{
	gchar *icon = NULL;

	g_return_val_if_fail (GPM_IS_ENGINE (engine), NULL);

	/* policy */
	if (engine->priv->icon_policy == GPM_ICON_POLICY_NEVER) {
		egg_debug ("no icon allowed, so no icon will be displayed.");
		return NULL;
	}

	/* we try CRITICAL: BATTERY, UPS, MOUSE, KEYBOARD */
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_BATTERY, GPM_ENGINE_WARNING_CRITICAL, FALSE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_UPS, GPM_ENGINE_WARNING_CRITICAL, FALSE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_MOUSE, GPM_ENGINE_WARNING_CRITICAL, FALSE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_KEYBOARD, GPM_ENGINE_WARNING_CRITICAL, FALSE);
	if (icon != NULL)
		return icon;

	/* policy */
	if (engine->priv->icon_policy == GPM_ICON_POLICY_CRITICAL) {
		egg_debug ("no devices critical, so no icon will be displayed.");
		return NULL;
	}

	/* we try CRITICAL: BATTERY, UPS, MOUSE, KEYBOARD */
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_BATTERY, GPM_ENGINE_WARNING_LOW, FALSE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_UPS, GPM_ENGINE_WARNING_LOW, FALSE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_MOUSE, GPM_ENGINE_WARNING_LOW, FALSE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_KEYBOARD, GPM_ENGINE_WARNING_LOW, FALSE);
	if (icon != NULL)
		return icon;

	/* policy */
	if (engine->priv->icon_policy == GPM_ICON_POLICY_LOW) {
		egg_debug ("no devices low, so no icon will be displayed.");
		return NULL;
	}

	/* we try (DIS)CHARGING: BATTERY, UPS */
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_BATTERY, GPM_ENGINE_WARNING_NONE, TRUE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_UPS, GPM_ENGINE_WARNING_NONE, TRUE);
	if (icon != NULL)
		return icon;

	/* policy */
	if (engine->priv->icon_policy == GPM_ICON_POLICY_CHARGE) {
		egg_debug ("no devices (dis)charging, so no icon will be displayed.");
		return NULL;
	}

	/* we try PRESENT: BATTERY, UPS */
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_BATTERY, GPM_ENGINE_WARNING_NONE, FALSE);
	if (icon != NULL)
		return icon;
	icon = gpm_engine_get_icon_priv (engine, UP_DEVICE_KIND_UPS, GPM_ENGINE_WARNING_NONE, FALSE);
	if (icon != NULL)
		return icon;

	/* policy */
	if (engine->priv->icon_policy == GPM_ICON_POLICY_PRESENT) {
		egg_debug ("no devices present, so no icon will be displayed.");
		return NULL;
	}

	/* we fallback to the ac_adapter icon */
	egg_debug ("Using fallback");
	return g_strdup (GPM_ICON_AC_ADAPTER);
}

/**
 * gpm_engine_recalculate_state_icon:
 */
static gboolean
gpm_engine_recalculate_state_icon (GpmEngine *engine)
{
	gchar *icon;

	g_return_val_if_fail (engine != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ENGINE (engine), FALSE);

	/* show a different icon if we are disconnected */
	icon = gpm_engine_get_icon (engine);
	if (icon == NULL) {
		/* none before, now none */
		if (engine->priv->previous_icon == NULL)
			return FALSE;
		/* icon before, now none */
		egg_debug ("** EMIT: icon-changed: none");
		g_signal_emit (engine, signals [ICON_CHANGED], 0, NULL);

		g_free (engine->priv->previous_icon);
		engine->priv->previous_icon = NULL;
		return TRUE;
	}

	/* no icon before, now icon */
	if (engine->priv->previous_icon == NULL) {
		egg_debug ("** EMIT: icon-changed: %s", icon);
		g_signal_emit (engine, signals [ICON_CHANGED], 0, icon);
		engine->priv->previous_icon = icon;
		return TRUE;
	}

	/* icon before, now different */
	if (strcmp (engine->priv->previous_icon, icon) != 0) {
		g_free (engine->priv->previous_icon);
		engine->priv->previous_icon = icon;
		egg_debug ("** EMIT: icon-changed: %s", icon);
		g_signal_emit (engine, signals [ICON_CHANGED], 0, icon);
		return TRUE;
	}

	egg_debug ("no change");
	/* nothing to do */
	g_free (icon);
	return FALSE;
}

/**
 * gpm_engine_recalculate_state_summary:
 */
static gboolean
gpm_engine_recalculate_state_summary (GpmEngine *engine)
{
	gchar *summary;

	summary = gpm_engine_get_summary (engine);
	if (engine->priv->previous_summary == NULL) {
		engine->priv->previous_summary = summary;
		egg_debug ("** EMIT: summary-changed(1): %s", summary);
		g_signal_emit (engine, signals [SUMMARY_CHANGED], 0, summary);
		return TRUE;
	}	

	if (strcmp (engine->priv->previous_summary, summary) != 0) {
		g_free (engine->priv->previous_summary);
		engine->priv->previous_summary = summary;
		egg_debug ("** EMIT: summary-changed(2): %s", summary);
		g_signal_emit (engine, signals [SUMMARY_CHANGED], 0, summary);
		return TRUE;
	}
	egg_debug ("no change");
	/* nothing to do */
	g_free (summary);
	return FALSE;
}

/**
 * gpm_engine_recalculate_state:
 */
static void
gpm_engine_recalculate_state (GpmEngine *engine)
{

	g_return_if_fail (engine != NULL);
	g_return_if_fail (GPM_IS_ENGINE (engine));

	gpm_engine_recalculate_state_icon (engine);
	gpm_engine_recalculate_state_summary (engine);

	g_signal_emit (engine, signals [DEVICES_CHANGED], 0);
}

/**
 * gpm_engine_settings_key_changed_cb:
 **/
static void
gpm_engine_settings_key_changed_cb (GSettings *settings, const gchar *key, GpmEngine *engine)
{

	if (g_strcmp0 (key, GPM_SETTINGS_USE_TIME_POLICY) == 0) {
		engine->priv->use_time_primary = g_settings_get_boolean (settings, key);

	} else if (g_strcmp0 (key, GPM_SETTINGS_ICON_POLICY) == 0) {

		/* do we want to display the icon in the tray */
		engine->priv->icon_policy = g_settings_get_enum (settings, key);

		/* perhaps change icon */
		gpm_engine_recalculate_state_icon (engine);
	}
}

/**
 * gpm_engine_device_check_capacity:
 **/
static gboolean
gpm_engine_device_check_capacity (GpmEngine *engine, UpDevice *device)
{
	gboolean ret;
	UpDeviceKind kind;
	gdouble capacity;

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      "capacity", &capacity,
		      NULL);

	/* not laptop battery */
	if (kind != UP_DEVICE_KIND_BATTERY)
		return FALSE;

	/* capacity okay */
	if (capacity > 50.0f)
		return FALSE;

	/* capacity invalid */
	if (capacity < 1.0f)
		return FALSE;

	/* only emit this if specified in the settings */
	ret = g_settings_get_boolean (engine->priv->settings, GPM_SETTINGS_NOTIFY_LOW_CAPACITY);
	if (ret) {
		egg_debug ("** EMIT: low-capacity");
		g_signal_emit (engine, signals [LOW_CAPACITY], 0, device);
	}
	return TRUE;
}

/**
 * gpm_engine_get_composite_device:
 **/
static UpDevice *
gpm_engine_get_composite_device (GpmEngine *engine, UpDevice *original_device)
{
	return engine->priv->battery_composite;
}

/**
 * gpm_engine_update_composite_device:
 **/
static UpDevice *
gpm_engine_update_composite_device (GpmEngine *engine, UpDevice *original_device)
{
	gchar *text;

	text = up_device_to_text (engine->priv->battery_composite);
	egg_debug ("composite:\n%s", text);
	g_free (text);

	/* force update of icon */
	gpm_engine_recalculate_state_icon (engine);

	return engine->priv->battery_composite;
}

/**
 * gpm_engine_device_add:
 **/
static void
gpm_engine_device_add (GpmEngine *engine, UpDevice *device)
{
	UpDeviceLevel warning;
	UpDeviceState state;
	UpDeviceKind kind;
	UpDevice *composite;

	/* assign warning */
	warning = gpm_engine_get_warning (engine, device);
	g_object_set_data (G_OBJECT(device), "engine-warning-old", GUINT_TO_POINTER(warning));

	/* check capacity */
	gpm_engine_device_check_capacity (engine, device);

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      "state", &state,
		      NULL);

	/* add old state for transitions */
	egg_debug ("adding %s with state %s", up_device_get_object_path (device), up_device_state_to_string (state));
	g_object_set_data (G_OBJECT(device), "engine-state-old", GUINT_TO_POINTER(state));

	if (kind == UP_DEVICE_KIND_BATTERY) {
		egg_debug ("updating because we added a device");
		composite = gpm_engine_update_composite_device (engine, device);

		/* get the same values for the composite device */
		warning = gpm_engine_get_warning (engine, composite);
		g_object_set_data (G_OBJECT(composite), "engine-warning-old", GUINT_TO_POINTER(warning));
		g_object_get (composite, "state", &state, NULL);
		g_object_set_data (G_OBJECT(composite), "engine-state-old", GUINT_TO_POINTER(state));
	}

	g_signal_connect (device, "notify", G_CALLBACK (gpm_engine_device_changed_cb), engine);
	g_ptr_array_add (engine->priv->array, g_object_ref (device));
	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_engine_coldplug_idle_cb:
 **/
static gboolean
gpm_engine_coldplug_idle_cb (GpmEngine *engine)
{
	guint i;
	GPtrArray *array = NULL;
	UpDevice *device;

	g_return_val_if_fail (engine != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_ENGINE (engine), FALSE);
	/* connected mobile phones */
	gpm_phone_coldplug (engine->priv->phone);

	gpm_engine_recalculate_state (engine);

	/* add to database */
	array = up_client_get_devices2 (engine->priv->client);
	for (i=0;i<array->len;i++) {
		device = g_ptr_array_index (array, i);
		gpm_engine_device_add (engine, device);
	}
	if (array != NULL)
		g_ptr_array_unref (array);
	/* never repeat */
	return FALSE;
}

/**
 * gpm_engine_device_added_cb:
 **/
static void
gpm_engine_device_added_cb (UpClient *client, UpDevice *device, GpmEngine *engine)
{
	gpm_engine_device_add (engine, device);
}

/**
 * gpm_engine_device_removed_cb:
 **/
static void
gpm_engine_device_removed_cb (UpClient *client, const char *object_path, GpmEngine *engine)
{
	guint i;

	for (i = 0; i < engine->priv->array->len; i++) {
		UpDevice *device = g_ptr_array_index (engine->priv->array, i);

		if (g_strcmp0 (object_path, up_device_get_object_path (device)) == 0) {
			g_ptr_array_remove_index (engine->priv->array, i);
			break;
		}
	}
	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_engine_device_changed_cb:
 **/
static void
gpm_engine_device_changed_cb (UpDevice *device, GParamSpec *pspec, GpmEngine *engine)
{
	UpDeviceKind kind;
	UpDeviceState state;
	UpDeviceState state_old;
	UpDeviceLevel warning_old;
	UpDeviceLevel warning;

	/* get device properties */
	g_object_get (device,
		      "kind", &kind,
		      NULL);

	/* if battery then use composite device to cope with multiple batteries */
	if (kind == UP_DEVICE_KIND_BATTERY) {
		egg_debug ("updating because %s changed", up_device_get_object_path (device));
		device = gpm_engine_update_composite_device (engine, device);
	}

	/* get device properties (may be composite) */
	g_object_get (device,
		      "state", &state,
		      NULL);

	egg_debug ("%s state is now %s", up_device_get_object_path (device), up_device_state_to_string (state));

	/* see if any interesting state changes have happened */
	state_old = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(device), "engine-state-old"));
	if (state_old != state) {
		if (state == UP_DEVICE_STATE_DISCHARGING) {
			egg_debug ("** EMIT: discharging");
			g_signal_emit (engine, signals [DISCHARGING], 0, device);
		} else if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
			egg_debug ("** EMIT: fully charged");
			g_signal_emit (engine, signals [FULLY_CHARGED], 0, device);
		}

		/* save new state */
		g_object_set_data (G_OBJECT(device), "engine-state-old", GUINT_TO_POINTER(state));
	}

	/* check the warning state has not changed */
	warning_old = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(device), "engine-warning-old"));
	warning = gpm_engine_get_warning (engine, device);
	if (warning != warning_old) {
		if (warning == GPM_ENGINE_WARNING_LOW) {
			egg_debug ("** EMIT: charge-low");
			g_signal_emit (engine, signals [CHARGE_LOW], 0, device);
		} else if (warning == GPM_ENGINE_WARNING_CRITICAL) {
			egg_debug ("** EMIT: charge-critical");
			g_signal_emit (engine, signals [CHARGE_CRITICAL], 0, device);
		} else if (warning == GPM_ENGINE_WARNING_ACTION) {
			egg_debug ("** EMIT: charge-action");
			g_signal_emit (engine, signals [CHARGE_ACTION], 0, device);
		}
		/* save new state */
		g_object_set_data (G_OBJECT(device), "engine-warning-old", GUINT_TO_POINTER(warning));
	}

	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_engine_get_devices:
 *
 * Return value: the UpDevice array, free with g_ptr_array_unref()
 **/
GPtrArray *
gpm_engine_get_devices (GpmEngine *engine)
{
	return g_ptr_array_ref (engine->priv->array);
}

/**
 * gpm_engine_get_primary_device:
 *
 * Return value: the #UpDevice, free with g_object_unref()
 **/
UpDevice *
gpm_engine_get_primary_device (GpmEngine *engine)
{
	guint i;
	UpDevice *device = NULL;
	UpDevice *device_tmp;
	UpDeviceKind kind;
	UpDeviceState state;
	gboolean is_present;

	for (i=0; i<engine->priv->array->len; i++) {
		device_tmp = g_ptr_array_index (engine->priv->array, i);

		/* get device properties */
		g_object_get (device_tmp,
			      "kind", &kind,
			      "state", &state,
			      "is-present", &is_present,
			      NULL);

		/* not present */
		if (!is_present)
			continue;

		/* not discharging */
		if (state != UP_DEVICE_STATE_DISCHARGING)
			continue;

		/* not battery */
		if (kind != UP_DEVICE_KIND_BATTERY)
			continue;

		/* use composite device to cope with multiple batteries */
		device = g_object_ref (gpm_engine_get_composite_device (engine, device_tmp));
		break;
	}
	return device;
}

/**
 * phone_device_added_cb:
 **/
static void
phone_device_added_cb (GpmPhone *phone, guint idx, GpmEngine *engine)
{
	UpDevice *device;
	device = up_device_new ();

	egg_debug ("phone added %i", idx);

	/* get device properties */
	g_object_set (device,
		      "kind", UP_DEVICE_KIND_PHONE,
		      "is-rechargeable", TRUE,
		      "native-path", g_strdup_printf ("dummy:phone_%i", idx),
		      "is-present", TRUE,
		      NULL);

	/* state changed */
	gpm_engine_device_add (engine, device);
	g_ptr_array_add (engine->priv->array, g_object_ref (device));
	gpm_engine_recalculate_state (engine);
}

/**
 * phone_device_removed_cb:
 **/
static void
phone_device_removed_cb (GpmPhone *phone, guint idx, GpmEngine *engine)
{
	guint i;
	UpDevice *device;
	UpDeviceKind kind;

	egg_debug ("phone removed %i", idx);

	for (i=0; i<engine->priv->array->len; i++) {
		device = g_ptr_array_index (engine->priv->array, i);

		/* get device properties */
		g_object_get (device,
			      "kind", &kind,
			      NULL);

		if (kind == UP_DEVICE_KIND_PHONE) {
			g_ptr_array_remove_index (engine->priv->array, i);
			break;
		}
	}

	/* state changed */
	gpm_engine_recalculate_state (engine);
}

/**
 * phone_device_refresh_cb:
 **/
static void
phone_device_refresh_cb (GpmPhone *phone, guint idx, GpmEngine *engine)
{
	guint i;
	UpDevice *device;
	UpDeviceKind kind;
	UpDeviceState state;
	gboolean is_present;
	gdouble percentage;

	egg_debug ("phone refresh %i", idx);

	for (i=0; i<engine->priv->array->len; i++) {
		device = g_ptr_array_index (engine->priv->array, i);

		/* get device properties */
		g_object_get (device,
			      "kind", &kind,
			      "state", &state,
			      "percentage", &percentage,
			      "is-present", &is_present,
			      NULL);

		if (kind == UP_DEVICE_KIND_PHONE) {
			is_present = gpm_phone_get_present (phone, idx);
			state = gpm_phone_get_on_ac (phone, idx) ? UP_DEVICE_STATE_CHARGING : UP_DEVICE_STATE_DISCHARGING;
			percentage = gpm_phone_get_percentage (phone, idx);
			break;
		}
	}

	/* state changed */
	gpm_engine_recalculate_state (engine);
}

/**
 * gpm_engine_init:
 * @engine: This class instance
 **/
static void
gpm_engine_init (GpmEngine *engine)
{
	guint idle_id;
	engine->priv = gpm_engine_get_instance_private (engine);

	engine->priv->array = g_ptr_array_new_with_free_func (g_object_unref);
	engine->priv->client = up_client_new ();
	g_signal_connect (engine->priv->client, "device-added",
			  G_CALLBACK (gpm_engine_device_added_cb), engine);
	g_signal_connect (engine->priv->client, "device-removed",
			  G_CALLBACK (gpm_engine_device_removed_cb), engine);

	engine->priv->settings = g_settings_new (GPM_SETTINGS_SCHEMA);
	g_signal_connect (engine->priv->settings, "changed",
			  G_CALLBACK (gpm_engine_settings_key_changed_cb), engine);

	engine->priv->phone = gpm_phone_new ();
	g_signal_connect (engine->priv->phone, "device-added",
			  G_CALLBACK (phone_device_added_cb), engine);
	g_signal_connect (engine->priv->phone, "device-removed",
			  G_CALLBACK (phone_device_removed_cb), engine);
	g_signal_connect (engine->priv->phone, "device-refresh",
			  G_CALLBACK (phone_device_refresh_cb), engine);

	/* create a fake virtual composite battery */
	engine->priv->battery_composite = up_client_get_display_device (engine->priv->client);
	g_signal_connect (engine->priv->battery_composite, "notify",
			  G_CALLBACK (gpm_engine_device_changed_cb), engine);

	engine->priv->previous_icon = NULL;
	engine->priv->previous_summary = NULL;

	/* do we want to display the icon in the tray */
	engine->priv->icon_policy = g_settings_get_enum (engine->priv->settings, GPM_SETTINGS_ICON_POLICY);

	/* get percentage policy */
	engine->priv->low_percentage = g_settings_get_int (engine->priv->settings, GPM_SETTINGS_PERCENTAGE_LOW);
	engine->priv->critical_percentage = g_settings_get_int (engine->priv->settings, GPM_SETTINGS_PERCENTAGE_CRITICAL);
	engine->priv->action_percentage = g_settings_get_int (engine->priv->settings, GPM_SETTINGS_PERCENTAGE_ACTION);

	/* get time policy */
	engine->priv->low_time = g_settings_get_int (engine->priv->settings, GPM_SETTINGS_TIME_LOW);
	engine->priv->critical_time = g_settings_get_int (engine->priv->settings, GPM_SETTINGS_TIME_CRITICAL);
	engine->priv->action_time = g_settings_get_int (engine->priv->settings, GPM_SETTINGS_TIME_ACTION);

	/* we can disable this if the time remaining is inaccurate or just plain wrong */
	engine->priv->use_time_primary = g_settings_get_boolean (engine->priv->settings, GPM_SETTINGS_USE_TIME_POLICY);
	if (engine->priv->use_time_primary)
		egg_debug ("Using per-time notification policy");
	else
		egg_debug ("Using percentage notification policy");

	idle_id = g_idle_add ((GSourceFunc) gpm_engine_coldplug_idle_cb, engine);
	g_source_set_name_by_id (idle_id, "[GpmEngine] coldplug");
}

/**
 * gpm_engine_class_init:
 * @engine: This class instance
 **/
static void
gpm_engine_class_init (GpmEngineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_engine_finalize;

	signals [ICON_CHANGED] =
		g_signal_new ("icon-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, icon_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals [SUMMARY_CHANGED] =
		g_signal_new ("summary-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, summary_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals [LOW_CAPACITY] =
		g_signal_new ("low-capacity",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, low_capacity),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [FULLY_CHARGED] =
		g_signal_new ("fully-charged",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, fully_charged),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [DISCHARGING] =
		g_signal_new ("discharging",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, discharging),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [CHARGE_ACTION] =
		g_signal_new ("charge-action",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, charge_action),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [CHARGE_LOW] =
		g_signal_new ("charge-low",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, charge_low),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [CHARGE_CRITICAL] =
		g_signal_new ("charge-critical",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, charge_critical),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [DEVICES_CHANGED] =
		g_signal_new ("devices-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmEngineClass, devices_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/**
 * gpm_engine_finalize:
 * @object: This class instance
 **/
static void
gpm_engine_finalize (GObject *object)
{
	GpmEngine *engine;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_ENGINE (object));

	engine = GPM_ENGINE (object);
	engine->priv = gpm_engine_get_instance_private (engine);

	g_ptr_array_unref (engine->priv->array);
	g_object_unref (engine->priv->client);
	g_object_unref (engine->priv->phone);
	g_object_unref (engine->priv->battery_composite);

	g_free (engine->priv->previous_icon);
	g_free (engine->priv->previous_summary);

	G_OBJECT_CLASS (gpm_engine_parent_class)->finalize (object);
}

/**
 * gpm_engine_new:
 * Return value: new class instance.
 **/
GpmEngine *
gpm_engine_new (void)
{
	if (gpm_engine_object != NULL) {
		g_object_ref (gpm_engine_object);
	} else {
		gpm_engine_object = g_object_new (GPM_TYPE_ENGINE, NULL);
		g_object_add_weak_pointer (gpm_engine_object, &gpm_engine_object);
	}
	return GPM_ENGINE (gpm_engine_object);

}

