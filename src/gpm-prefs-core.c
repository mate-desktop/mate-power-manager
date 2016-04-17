/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2009 Richard Hughes <richard@hughsie.com>
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

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <math.h>
#include <string.h>
#define UPOWER_ENABLE_DEPRECATED
#include <libupower-glib/upower.h>

#include "egg-debug.h"
#include "egg-console-kit.h"

#include "gpm-tray-icon.h"
#include "gpm-common.h"
#include "gpm-prefs-core.h"
#include "gpm-icon-names.h"
#include "gpm-brightness.h"

static void gpm_prefs_finalize (GObject *object);

#define GPM_PREFS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_PREFS, GpmPrefsPrivate))

struct GpmPrefsPrivate
{
	UpClient		*client;
	GtkBuilder		*builder;
	gboolean		 has_batteries;
	gboolean		 has_lcd;
	gboolean		 has_ups;
	gboolean		 has_button_lid;
	gboolean		 has_button_suspend;
	gboolean		 can_shutdown;
	gboolean		 can_suspend;
	gboolean		 can_hibernate;
	GSettings		*settings;
	EggConsoleKit		*console;
};

enum {
	ACTION_HELP,
	ACTION_CLOSE,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmPrefs, gpm_prefs, G_TYPE_OBJECT)

/**
 * gpm_prefs_class_init:
 * @klass: This prefs class instance
 **/
static void
gpm_prefs_class_init (GpmPrefsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_prefs_finalize;
	g_type_class_add_private (klass, sizeof (GpmPrefsPrivate));

	signals [ACTION_HELP] =
		g_signal_new ("action-help",
				  G_TYPE_FROM_CLASS (object_class),
				  G_SIGNAL_RUN_LAST,
				  G_STRUCT_OFFSET (GpmPrefsClass, action_help),
				  NULL,
				  NULL,
				  g_cclosure_marshal_VOID__VOID,
				  G_TYPE_NONE, 0);
	signals [ACTION_CLOSE] =
		g_signal_new ("action-close",
				  G_TYPE_FROM_CLASS (object_class),
				  G_SIGNAL_RUN_LAST,
				  G_STRUCT_OFFSET (GpmPrefsClass, action_close),
				  NULL,
				  NULL,
				  g_cclosure_marshal_VOID__VOID,
				  G_TYPE_NONE, 0);
}

/**
 * gpm_prefs_activate_window:
 * @prefs: This prefs class instance
 *
 * Activates (shows) the window.
 **/
void
gpm_prefs_activate_window (GtkApplication *app, GpmPrefs *prefs)
{
	GtkWindow *window;
	window = GTK_WINDOW (gtk_builder_get_object (prefs->priv->builder, "dialog_preferences"));
	gtk_application_add_window (GTK_APPLICATION (app), window);
	gtk_window_present (window);
}

/**
 * gpm_prefs_help_cb:
 * @widget: The GtkWidget object
 * @prefs: This prefs class instance
 **/
static void
gpm_prefs_help_cb (GtkWidget *widget, GpmPrefs *prefs)
{
	egg_debug ("emitting action-help");
	g_signal_emit (prefs, signals [ACTION_HELP], 0);
}

/**
 * gpm_prefs_icon_radio_cb:
 * @widget: The GtkWidget object
 **/
static void
gpm_prefs_icon_radio_cb (GtkWidget *widget, GpmPrefs *prefs)
{
	gint policy;

	policy = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "policy"));
	g_settings_set_enum (prefs->priv->settings, GPM_SETTINGS_ICON_POLICY, policy);
}

/**
 * gpm_prefs_format_percentage_cb:
 * @scale: The GtkScale object
 * @value: The value in %.
 **/
static gchar *
gpm_prefs_format_percentage_cb (GtkScale *scale, gdouble value)
{
	return g_strdup_printf ("%.0f%%", value);
}

/**
 * gpm_prefs_action_combo_changed_cb:
 **/
static void
gpm_prefs_action_combo_changed_cb (GtkWidget *widget, GpmPrefs *prefs)
{
	GpmActionPolicy policy;
	const GpmActionPolicy *actions;
	const gchar *gpm_pref_key;
	guint active;

	actions = (const GpmActionPolicy *) g_object_get_data (G_OBJECT (widget), "actions");
	gpm_pref_key = (const gchar *) g_object_get_data (G_OBJECT (widget), "settings_key");

	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	policy = actions[active];
	g_settings_set_enum (prefs->priv->settings, gpm_pref_key, policy);
}

/**
 * gpm_prefs_action_time_changed_cb:
 **/
static void
gpm_prefs_action_time_changed_cb (GtkWidget *widget, GpmPrefs *prefs)
{
	guint value;
	const gint *values;
	const gchar *gpm_pref_key;
	guint active;

	values = (const gint *) g_object_get_data (G_OBJECT (widget), "values");
	gpm_pref_key = (const gchar *) g_object_get_data (G_OBJECT (widget), "settings_key");

	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	value = values[active];

	egg_debug ("Changing %s to %i", gpm_pref_key, value);
	g_settings_set_int (prefs->priv->settings, gpm_pref_key, value);
}

/**
 * gpm_prefs_actions_destroy_cb:
 **/
static void
gpm_prefs_actions_destroy_cb (GpmActionPolicy *array)
{
	g_free (array);
}

/**
 * gpm_prefs_setup_action_combo:
 * @prefs: This prefs class instance
 * @widget_name: The GtkWidget name
 * @gpm_pref_key: The settings key for this preference setting.
 * @actions: The actions to associate in an array.
 **/
static void
gpm_prefs_setup_action_combo (GpmPrefs *prefs, const gchar *widget_name,
				  const gchar *gpm_pref_key, const GpmActionPolicy *actions)
{
	gint i;
	gboolean is_writable;
	GtkWidget *widget;
	GpmActionPolicy policy;
	GpmActionPolicy	value;
	GPtrArray *array;
	GpmActionPolicy *actions_added;

	widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, widget_name));

	value = g_settings_get_enum (prefs->priv->settings, gpm_pref_key);
	is_writable = g_settings_is_writable (prefs->priv->settings, gpm_pref_key);

	gtk_widget_set_sensitive (widget, is_writable);

	array = g_ptr_array_new ();
	g_object_set_data (G_OBJECT (widget), "settings_key", (gpointer) gpm_pref_key);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_prefs_action_combo_changed_cb), prefs);

	for (i=0; actions[i] != -1; i++) {
		policy = actions[i];
		if (policy == GPM_ACTION_POLICY_SHUTDOWN && !prefs->priv->can_shutdown) {
			egg_debug ("Cannot add option, as cannot shutdown.");
		} else if (policy == GPM_ACTION_POLICY_SHUTDOWN && prefs->priv->can_shutdown) {
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (widget), _("Shutdown"));
			g_ptr_array_add(array, GINT_TO_POINTER (policy));
		} else if (policy == GPM_ACTION_POLICY_SUSPEND && !prefs->priv->can_suspend) {
			egg_debug ("Cannot add option, as cannot suspend.");
		} else if (policy == GPM_ACTION_POLICY_HIBERNATE && !prefs->priv->can_hibernate) {
			egg_debug ("Cannot add option, as cannot hibernate.");
		} else if (policy == GPM_ACTION_POLICY_SUSPEND && prefs->priv->can_suspend) {
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (widget), _("Suspend"));
			g_ptr_array_add (array, GINT_TO_POINTER (policy));
		} else if (policy == GPM_ACTION_POLICY_HIBERNATE && prefs->priv->can_hibernate) {
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (widget), _("Hibernate"));
			g_ptr_array_add(array, GINT_TO_POINTER (policy));
		} else if (policy == GPM_ACTION_POLICY_BLANK) {
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (widget), _("Blank screen"));
			g_ptr_array_add (array, GINT_TO_POINTER (policy));
		} else if (policy == GPM_ACTION_POLICY_INTERACTIVE) {
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (widget), _("Ask me"));
			g_ptr_array_add(array, GINT_TO_POINTER (policy));
		} else if (policy == GPM_ACTION_POLICY_NOTHING) {
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (widget), _("Do nothing"));
			g_ptr_array_add(array, GINT_TO_POINTER (policy));
		} else {
			egg_warning ("Unknown action read from settings: %i", policy);
		}
	}

	/* save as array _only_ the actions we could add */
	actions_added = g_new0 (GpmActionPolicy, array->len+1);
	for (i=0; i<array->len; i++)
		actions_added[i] = GPOINTER_TO_INT (g_ptr_array_index (array, i));
	actions_added[i] = -1;

	g_object_set_data_full (G_OBJECT (widget), "actions", (gpointer) actions_added, (GDestroyNotify) gpm_prefs_actions_destroy_cb);

	/* set what we have in the settings */
	for (i=0; actions_added[i] != -1; i++) {
		policy = actions_added[i];
		if (value == policy)
			 gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
	}

	g_ptr_array_unref (array);
}

/**
 * gpm_prefs_setup_time_combo:
 * @prefs: This prefs class instance
 * @widget_name: The GtkWidget name
 * @gpm_pref_key: The settings key for this preference setting.
 * @actions: The actions to associate in an array.
 **/
static void
gpm_prefs_setup_time_combo (GpmPrefs *prefs, const gchar *widget_name,
				const gchar *gpm_pref_key, const gint *values)
{
	guint value;
	gchar *text;
	guint i;
	gboolean is_writable;
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, widget_name));

	value = g_settings_get_int (prefs->priv->settings, gpm_pref_key);
	is_writable = g_settings_is_writable (prefs->priv->settings, gpm_pref_key);
	gtk_widget_set_sensitive (widget, is_writable);

	g_object_set_data (G_OBJECT (widget), "settings_key", (gpointer) gpm_pref_key);
	g_object_set_data (G_OBJECT (widget), "values", (gpointer) values);

	/* add each time */
	for (i=0; values[i] != -1; i++) {

		/* get translation for number of seconds */
		if (values[i] != 0) {
			text = gpm_get_timestring (values[i]);
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (widget), text);
			g_free (text);
		} else {
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (widget), _("Never"));
		}

		/* matches, so set default */
		if (value == values[i])
			 gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
	}

	/* connect after set */
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gpm_prefs_action_time_changed_cb), prefs);
}

/**
 * gpm_prefs_close_cb:
 * @widget: The GtkWidget object
 * @prefs: This prefs class instance
 **/
static void
gpm_prefs_close_cb (GtkWidget *widget, GpmPrefs *prefs)
{
	egg_debug ("emitting action-close");
	g_signal_emit (prefs, signals [ACTION_CLOSE], 0);
}

/**
 * gpm_prefs_delete_event_cb:
 * @widget: The GtkWidget object
 * @event: The event type, unused.
 * @prefs: This prefs class instance
 **/
static gboolean
gpm_prefs_delete_event_cb (GtkWidget *widget, GdkEvent *event, GpmPrefs *prefs)
{
	gpm_prefs_close_cb (widget, prefs);
	return FALSE;
}

/** setup the notification page */
static void
prefs_setup_notification (GpmPrefs *prefs)
{
	gint icon_policy;
	GtkWidget *radiobutton_icon_always;
	GtkWidget *radiobutton_icon_present;
	GtkWidget *radiobutton_icon_charge;
	GtkWidget *radiobutton_icon_low;
	GtkWidget *radiobutton_icon_never;
	gboolean is_writable;

	icon_policy = g_settings_get_enum (prefs->priv->settings, GPM_SETTINGS_ICON_POLICY);

	radiobutton_icon_always = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
						  "radiobutton_notification_always"));
	radiobutton_icon_present = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
						   "radiobutton_notification_present"));
	radiobutton_icon_charge = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
						  "radiobutton_notification_charge"));
	radiobutton_icon_low = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
					   "radiobutton_notification_low"));
	radiobutton_icon_never = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
						 "radiobutton_notification_never"));

	is_writable = g_settings_is_writable (prefs->priv->settings, GPM_SETTINGS_ICON_POLICY);
	gtk_widget_set_sensitive (radiobutton_icon_always, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_present, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_charge, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_low, is_writable);
	gtk_widget_set_sensitive (radiobutton_icon_never, is_writable);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_always),
					  icon_policy == GPM_ICON_POLICY_ALWAYS);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_present),
					  icon_policy == GPM_ICON_POLICY_PRESENT);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_charge),
					  icon_policy == GPM_ICON_POLICY_CHARGE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_low),
					  icon_policy == GPM_ICON_POLICY_LOW);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_icon_never),
					  icon_policy == GPM_ICON_POLICY_NEVER);

	g_object_set_data (G_OBJECT (radiobutton_icon_always), "policy",
			   GINT_TO_POINTER (GPM_ICON_POLICY_ALWAYS));
	g_object_set_data (G_OBJECT (radiobutton_icon_present), "policy",
			   GINT_TO_POINTER (GPM_ICON_POLICY_PRESENT));
	g_object_set_data (G_OBJECT (radiobutton_icon_charge), "policy",
			   GINT_TO_POINTER (GPM_ICON_POLICY_CHARGE));
	g_object_set_data (G_OBJECT (radiobutton_icon_low), "policy",
			   GINT_TO_POINTER (GPM_ICON_POLICY_LOW));
	g_object_set_data (G_OBJECT (radiobutton_icon_never), "policy",
			   GINT_TO_POINTER (GPM_ICON_POLICY_NEVER));

	/* only connect the callbacks after we set the value, else the settings
	 * keys gets written to (for a split second), and the icon flickers. */
	g_signal_connect (radiobutton_icon_always, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), prefs);
	g_signal_connect (radiobutton_icon_present, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), prefs);
	g_signal_connect (radiobutton_icon_charge, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), prefs);
	g_signal_connect (radiobutton_icon_low, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), prefs);
	g_signal_connect (radiobutton_icon_never, "clicked",
			  G_CALLBACK (gpm_prefs_icon_radio_cb), prefs);
}

static void
prefs_setup_ac (GpmPrefs *prefs)
{
	GtkWidget *widget;
	const GpmActionPolicy button_lid_actions[] =
				{GPM_ACTION_POLICY_NOTHING,
				 GPM_ACTION_POLICY_BLANK,
				 GPM_ACTION_POLICY_SUSPEND,
				 GPM_ACTION_POLICY_HIBERNATE,
				 GPM_ACTION_POLICY_SHUTDOWN,
				 -1};

	static const gint computer_times[] =
		{10*60,
		 30*60,
		 1*60*60,
		 2*60*60,
		 0, /* never */
		 -1};
	static const gint display_times[] =
		{1*60,
		 5*60,
		 10*60,
		 30*60,
		 1*60*60,
		 0, /* never */
		 -1};

	gpm_prefs_setup_time_combo (prefs, "combobox_ac_computer",
					GPM_SETTINGS_SLEEP_COMPUTER_AC,
					computer_times);
	gpm_prefs_setup_time_combo (prefs, "combobox_ac_display",
					GPM_SETTINGS_SLEEP_DISPLAY_AC,
					display_times);

	gpm_prefs_setup_action_combo (prefs, "combobox_ac_lid",
					  GPM_SETTINGS_BUTTON_LID_AC,
					  button_lid_actions);

	/* setup brightness slider */
	widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "hscale_ac_brightness"));
	g_settings_bind (prefs->priv->settings, GPM_SETTINGS_BRIGHTNESS_AC,
			 gtk_range_get_adjustment (GTK_RANGE (widget)), "value",
			 G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (G_OBJECT (widget), "format-value",
			  G_CALLBACK (gpm_prefs_format_percentage_cb), NULL);

	/* set up the checkboxes */
	widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "checkbutton_ac_display_dim"));
	g_settings_bind (prefs->priv->settings, GPM_SETTINGS_IDLE_DIM_AC,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);

	if (prefs->priv->has_button_lid == FALSE) {
		widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "hbox_ac_lid"));
		gtk_widget_hide(widget);
	}
	if (prefs->priv->has_lcd == FALSE) {
		widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "hbox_ac_brightness"));

		gtk_widget_hide(widget);
		
		widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "checkbutton_ac_display_dim"));

		gtk_widget_hide(widget);
	}
}

static void
prefs_setup_battery (GpmPrefs *prefs)
{
	GtkWidget *widget;
	GtkNotebook *notebook;
	gint page;

	const GpmActionPolicy button_lid_actions[] =
				{GPM_ACTION_POLICY_NOTHING,
				 GPM_ACTION_POLICY_BLANK,
				 GPM_ACTION_POLICY_SUSPEND,
				 GPM_ACTION_POLICY_HIBERNATE,
				 GPM_ACTION_POLICY_SHUTDOWN,
				 -1};
	const GpmActionPolicy battery_critical_actions[] =
				{GPM_ACTION_POLICY_NOTHING,
				 GPM_ACTION_POLICY_SUSPEND,
				 GPM_ACTION_POLICY_HIBERNATE,
				 GPM_ACTION_POLICY_SHUTDOWN,
				 -1};

	static const gint computer_times[] =
		{10*60,
		 30*60,
		 1*60*60,
		 2*60*60,
		 0, /* never */
		 -1};
	static const gint display_times[] =
		{1*60,
		 5*60,
		 10*60,
		 30*60,
		 1*60*60,
		 0, /* never */
		 -1};

	gpm_prefs_setup_time_combo (prefs, "combobox_battery_computer",
					GPM_SETTINGS_SLEEP_COMPUTER_BATT,
					computer_times);
	gpm_prefs_setup_time_combo (prefs, "combobox_battery_display",
					GPM_SETTINGS_SLEEP_DISPLAY_BATT,
					display_times);

	if (prefs->priv->has_batteries == FALSE) {
		notebook = GTK_NOTEBOOK (gtk_builder_get_object (prefs->priv->builder, "notebook_preferences"));
		widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "vbox_battery"));
		page = gtk_notebook_page_num (notebook, GTK_WIDGET (widget));
		gtk_notebook_remove_page (notebook, page);
		return;
	}

	gpm_prefs_setup_action_combo (prefs, "combobox_battery_lid",
					  GPM_SETTINGS_BUTTON_LID_BATT,
					  button_lid_actions);
	gpm_prefs_setup_action_combo (prefs, "combobox_battery_critical",
					  GPM_SETTINGS_ACTION_CRITICAL_BATT,
					  battery_critical_actions);

	/* set up the checkboxes */
	widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "checkbutton_battery_display_reduce"));
	g_settings_bind (prefs->priv->settings, GPM_SETTINGS_BACKLIGHT_BATTERY_REDUCE,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);
	widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "checkbutton_battery_display_dim"));
	g_settings_bind (prefs->priv->settings, GPM_SETTINGS_IDLE_DIM_BATT,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);

	if (prefs->priv->has_button_lid == FALSE) {
		widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "hbox_battery_lid"));

		gtk_widget_hide(widget);
	}
	if (prefs->priv->has_lcd == FALSE) {
		widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "checkbutton_battery_display_dim"));
		gtk_widget_hide(widget);
	}
}

static void
prefs_setup_ups (GpmPrefs *prefs)
{
	GtkWidget *widget;
	GtkNotebook *notebook;
	gint page;

	const GpmActionPolicy ups_low_actions[] =
				{GPM_ACTION_POLICY_NOTHING,
				 GPM_ACTION_POLICY_HIBERNATE,
				 GPM_ACTION_POLICY_SHUTDOWN,
				 -1};

	static const gint computer_times[] =
		{10*60,
		 30*60,
		 1*60*60,
		 2*60*60,
		 0, /* never */
		 -1};
	static const gint display_times[] =
		{1*60,
		 5*60,
		 10*60,
		 30*60,
		 1*60*60,
		 0, /* never */
		 -1};

	gpm_prefs_setup_time_combo (prefs, "combobox_ups_computer",
					GPM_SETTINGS_SLEEP_COMPUTER_UPS,
					computer_times);
	gpm_prefs_setup_time_combo (prefs, "combobox_ups_display",
					GPM_SETTINGS_SLEEP_DISPLAY_UPS,
					display_times);

	if (prefs->priv->has_ups == FALSE) {
		notebook = GTK_NOTEBOOK (gtk_builder_get_object (prefs->priv->builder, "notebook_preferences"));
		widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "vbox_ups"));
		page = gtk_notebook_page_num (notebook, GTK_WIDGET (widget));
		gtk_notebook_remove_page (notebook, page);
		return;
	}

	gpm_prefs_setup_action_combo (prefs, "combobox_ups_low",
					  GPM_SETTINGS_ACTION_LOW_UPS,
					  ups_low_actions);
	gpm_prefs_setup_action_combo (prefs, "combobox_ups_critical",
					  GPM_SETTINGS_ACTION_CRITICAL_UPS,
					  ups_low_actions);
}

static void
prefs_setup_general (GpmPrefs *prefs)
{
	GtkWidget *widget;
	const GpmActionPolicy power_button_actions[] =
				{GPM_ACTION_POLICY_INTERACTIVE,
				 GPM_ACTION_POLICY_SUSPEND,
				 GPM_ACTION_POLICY_HIBERNATE,
				 GPM_ACTION_POLICY_SHUTDOWN,
				 GPM_ACTION_POLICY_NOTHING,
				 -1};
	const GpmActionPolicy suspend_button_actions[] =
				{GPM_ACTION_POLICY_NOTHING,
				 GPM_ACTION_POLICY_SUSPEND,
				 GPM_ACTION_POLICY_HIBERNATE,
				 -1};

	gpm_prefs_setup_action_combo (prefs, "combobox_general_power",
					  GPM_SETTINGS_BUTTON_POWER,
					  power_button_actions);
	gpm_prefs_setup_action_combo (prefs, "combobox_general_suspend",
					  GPM_SETTINGS_BUTTON_SUSPEND,
					  suspend_button_actions);

	if (prefs->priv->has_button_suspend == FALSE) {
		widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "hbox_general_suspend"));

		gtk_widget_hide(widget);
	}
}

/**
 * gpm_prefs_init:
 * @prefs: This prefs class instance
 **/
static void
gpm_prefs_init (GpmPrefs *prefs)
{
	GtkWidget *main_window;
	GtkWidget *widget;
	guint retval;
	GError *error = NULL;
	GPtrArray *devices = NULL;
	UpDevice *device;
	UpDeviceKind kind;
	GpmBrightness *brightness;
#if !UP_CHECK_VERSION(0, 99, 0)
	gboolean ret;
#endif
	guint i;

	GDBusProxy *proxy;
	GVariant *res, *inner;
	gchar * r;

	prefs->priv = GPM_PREFS_GET_PRIVATE (prefs);

	prefs->priv->client = up_client_new ();
	prefs->priv->console = egg_console_kit_new ();
	prefs->priv->settings = g_settings_new (GPM_SETTINGS_SCHEMA);

	prefs->priv->can_shutdown = FALSE;
	prefs->priv->can_suspend = FALSE;
	prefs->priv->can_hibernate = FALSE;

	if (LOGIND_RUNNING()) {
		/* get values from logind */

		proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
						       NULL,
						       "org.freedesktop.login1",
						       "/org/freedesktop/login1",
						       "org.freedesktop.login1.Manager",
						       NULL,
						       &error );
		if (proxy == NULL) {
			egg_error("Error connecting to dbus - %s", error->message);
			g_error_free (error);
			return;
		}

		res = g_dbus_proxy_call_sync (proxy, "CanPowerOff",
					      NULL,
					      G_DBUS_CALL_FLAGS_NONE,
					      -1,
					      NULL,
					      &error
					      );
		if (error == NULL && res != NULL) {
			g_variant_get(res,"(&s)", &r);
			prefs->priv->can_shutdown = g_strcmp0(r,"yes")==0?TRUE:FALSE;
			g_variant_unref (res);
		} else if (error != NULL ) {
			egg_error ("Error in dbus - %s", error->message);
			g_error_free (error);
		}

		res = g_dbus_proxy_call_sync (proxy, "CanSuspend", 
					      NULL,
					      G_DBUS_CALL_FLAGS_NONE,
					      -1,
					      NULL,
					      &error
					      );
		if (error == NULL && res != NULL) {
			g_variant_get(res,"(&s)", &r);
			prefs->priv->can_suspend = g_strcmp0(r,"yes")==0?TRUE:FALSE;
			g_variant_unref (res);
		} else if (error != NULL ) {
			egg_error ("Error in dbus - %s", error->message);
			g_error_free (error);
		}

		res = g_dbus_proxy_call_sync (proxy, "CanHibernate", 
					      NULL,
					      G_DBUS_CALL_FLAGS_NONE,
					      -1,
					      NULL,
					      &error
					      );
		if (error == NULL && res != NULL) {
			g_variant_get(res,"(&s)", &r);
			prefs->priv->can_hibernate = g_strcmp0(r,"yes")==0?TRUE:FALSE;
			g_variant_unref (res);
		} else if (error != NULL ) {
			egg_error ("Error in dbus - %s", error->message);
			g_error_free (error);
		}
		g_object_unref(proxy);
	}
	else {
		/* Get values from ConsoleKit */
		egg_console_kit_can_stop (prefs->priv->console, &prefs->priv->can_shutdown, NULL);
		egg_console_kit_can_suspend (prefs->priv->console, &prefs->priv->can_suspend, NULL);
		egg_console_kit_can_hibernate (prefs->priv->console, &prefs->priv->can_hibernate, NULL);
	}

	if (LOGIND_RUNNING()) {
		proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
						       NULL,
						       "org.freedesktop.UPower",
						       "/org/freedesktop/UPower",
						       "org.freedesktop.DBus.Properties",
						       NULL,
						       &error );
		if (proxy == NULL) {
			egg_error("Error connecting to dbus - %s", error->message);
			g_error_free (error);
			return;
		}

		res = g_dbus_proxy_call_sync (proxy, "Get", 
					      g_variant_new( "(ss)", 
							     "org.freedesktop.UPower",
							     "LidIsPresent"),
					      G_DBUS_CALL_FLAGS_NONE,
					      -1,
					      NULL,
					      &error
					      );
		if (error == NULL && res != NULL) {
			g_variant_get(res, "(v)", &inner );
			prefs->priv->has_button_lid = g_variant_get_boolean(inner);
			g_variant_unref (inner);
			g_variant_unref (res);
		} else if (error != NULL ) {
			egg_error ("Error in dbus - %s", error->message);
			g_error_free (error);
		}
		g_object_unref(proxy);
	}
	else {
		prefs->priv->has_button_lid = up_client_get_lid_is_present (prefs->priv->client);
	}

	prefs->priv->has_button_suspend = TRUE;

	/* find if we have brightness hardware */
	brightness = gpm_brightness_new ();
	prefs->priv->has_lcd = gpm_brightness_has_hw (brightness);
	g_object_unref (brightness);
#if !UP_CHECK_VERSION(0, 99, 0)
	/* get device list */
	ret = up_client_enumerate_devices_sync (prefs->priv->client, NULL, &error);
	if (!ret) {
		egg_warning ("failed to get device list: %s", error->message);
		g_error_free (error);
	}
#endif
	devices = up_client_get_devices (prefs->priv->client);
	for (i=0; i<devices->len; i++) {
		device = g_ptr_array_index (devices, i);
		g_object_get (device,
			      "kind", &kind,
			      NULL);
		if (kind == UP_DEVICE_KIND_BATTERY)
			prefs->priv->has_batteries = TRUE;
		if (kind == UP_DEVICE_KIND_UPS)
			prefs->priv->has_ups = TRUE;
	}
	g_ptr_array_unref (devices);

	error = NULL;
	prefs->priv->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (prefs->priv->builder, GPM_DATA "/gpm-prefs.ui", &error);

	if (error) {
		egg_error ("failed to load ui: %s", error->message);
	}

	main_window = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "dialog_preferences"));

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);
	gtk_window_set_default_icon_name (GPM_ICON_APP_ICON);

	/* Get the main window quit */
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gpm_prefs_delete_event_cb), prefs);

	widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "button_close"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_prefs_close_cb), prefs);

	widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "button_help"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gpm_prefs_help_cb), prefs);

	widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "button_defaults"));
	gtk_widget_hide (widget);

	prefs_setup_ac (prefs);
	prefs_setup_battery (prefs);
	prefs_setup_ups (prefs);
	prefs_setup_general (prefs);
	prefs_setup_notification (prefs);
}

/**
 * gpm_prefs_finalize:
 * @object: This prefs class instance
 **/
static void
gpm_prefs_finalize (GObject *object)
{
	GpmPrefs *prefs;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_PREFS (object));

	prefs = GPM_PREFS (object);
	prefs->priv = GPM_PREFS_GET_PRIVATE (prefs);

	g_object_unref (prefs->priv->settings);
	g_object_unref (prefs->priv->client);
	g_object_unref (prefs->priv->console);
	g_object_unref (prefs->priv->builder);

	G_OBJECT_CLASS (gpm_prefs_parent_class)->finalize (object);
}

/**
 * gpm_prefs_new:
 * Return value: new GpmPrefs instance.
 **/
GpmPrefs *
gpm_prefs_new (void)
{
	GpmPrefs *prefs;
	prefs = g_object_new (GPM_TYPE_PREFS, NULL);
	return GPM_PREFS (prefs);
}

/**
 * gpm_window:
 * Return value: Prefs window widget.
 **/
GtkWidget *
gpm_window (GpmPrefs *prefs)
{
	return GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder, "dialog_preferences"));
}
