/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include <pk-debug.h>
#include <pk-job-list.h>
#include <pk-client.h>
#include <pk-common.h>
#include <pk-task-list.h>
#include <pk-connection.h>
#include <pk-package-id.h>
#include <pk-package-ids.h>
#include <pk-package-list.h>

#include "gpk-smart-icon.h"
#include "gpk-auto-refresh.h"
#include "gpk-common.h"
#include "gpk-notify.h"

static void     pk_notify_class_init	(PkNotifyClass *klass);
static void     pk_notify_init		(PkNotify      *notify);
static void     pk_notify_finalize	(GObject       *object);

#define PK_NOTIFY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PK_TYPE_NOTIFY, PkNotifyPrivate))

struct PkNotifyPrivate
{
	PkSmartIcon		*sicon;
	PkConnection		*pconnection;
	PkClient		*client_update_system;
	PkTaskList		*tlist;
	PkAutoRefresh		*arefresh;
	GConfClient		*gconf_client;
	gboolean		 cache_okay;
	gboolean		 cache_update_in_progress;
};

G_DEFINE_TYPE (PkNotify, pk_notify, G_TYPE_OBJECT)

/**
 * pk_notify_class_init:
 * @klass: The PkNotifyClass
 **/
static void
pk_notify_class_init (PkNotifyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = pk_notify_finalize;

	g_type_class_add_private (klass, sizeof (PkNotifyPrivate));
}

#if 0
/* No help yet */
/**
 * pk_notify_show_help_cb:
 **/
static void
pk_notify_show_help_cb (GtkMenuItem *item, PkNotify *notify)
{
	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));
	pk_debug ("show help");
	pk_smart_icon_notify_new (notify->priv->sicon,
			      _("Functionality incomplete"),
			      _("No help yet, sorry..."), "help-browser",
			      PK_NOTIFY_URGENCY_LOW, PK_NOTIFY_TIMEOUT_SHORT);
	pk_smart_icon_notify_button (notify->priv->sicon, PK_NOTIFY_BUTTON_DO_NOT_SHOW_AGAIN, PK_CONF_NOTIFY_ERROR);
	pk_smart_icon_notify_show (notify->priv->sicon);
}
#endif

/**
 * pk_notify_show_preferences_cb:
 **/
static void
pk_notify_show_preferences_cb (GtkMenuItem *item, PkNotify *notify)
{
	const gchar *command = "pk-prefs";
	if (g_spawn_command_line_async (command, NULL) == FALSE) {
		pk_warning ("Couldn't execute command: %s", command);
	}
}

/**
 * pk_notify_about_dialog_url_cb:
 **/
static void 
pk_notify_about_dialog_url_cb (GtkAboutDialog *about, const char *address, gpointer data)
{
	GError *error = NULL;
	gboolean ret;
	char *cmdline;
	GdkScreen *gscreen;
	GtkWidget *error_dialog;
	gchar *url;
	gchar *protocol = (gchar*) data;

	if (protocol != NULL)
		url = g_strconcat (protocol, address, NULL);
	else
		url = g_strdup (address);

	gscreen = gtk_window_get_screen (GTK_WINDOW (about));

	cmdline = g_strconcat ("xdg-open ", url, NULL);
	ret = gdk_spawn_command_line_on_screen (gscreen, cmdline, &error);
	g_free (cmdline);

	if (ret)
		goto out;

	g_error_free (error);
	error = NULL;

	cmdline = g_strconcat ("gnome-open ", url, NULL);
	ret = gdk_spawn_command_line_on_screen (gscreen, cmdline, &error);
	g_free (cmdline);
        
	if (ret == FALSE) {
		error_dialog = gtk_message_dialog_new (GTK_WINDOW (about),
						       GTK_DIALOG_MODAL,
						       GTK_MESSAGE_INFO,
						       GTK_BUTTONS_OK,
						       _("Failed to show url"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (error_dialog),
							  "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (error_dialog));
		gtk_widget_destroy (error_dialog);
		g_error_free (error);
	}

out:
	g_free (url);
}

/**
 * pk_notify_show_about_cb:
 **/
static void
pk_notify_show_about_cb (GtkMenuItem *item, gpointer data)
{
	static gboolean been_here = FALSE;
	const char *authors[] = {
		"Richard Hughes <richard@hughsie.com>",
		NULL};
	const char *documenters[] = {
		"Richard Hughes <richard@hughsie.com>",
		NULL};
	const char *license[] = {
		N_("Licensed under the GNU General Public License Version 2"),
		N_("PackageKit is free software; you can redistribute it and/or\n"
		   "modify it under the terms of the GNU General Public License\n"
		   "as published by the Free Software Foundation; either version 2\n"
		   "of the License, or (at your option) any later version."),
		N_("PackageKit is distributed in the hope that it will be useful,\n"
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License\n"
		   "along with this program; if not, write to the Free Software\n"
		   "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA\n"
		   "02110-1301, USA.")
	};
  	const char  *translators = _("translator-credits");
	char	    *license_trans;

	/* Translators comment: put your own name here to appear in the about dialog. */
  	if (!strcmp (translators, "translator-credits")) {
		translators = NULL;
	}

	license_trans = g_strconcat (_(license[0]), "\n\n", _(license[1]), "\n\n",
				     _(license[2]), "\n\n", _(license[3]), "\n",  NULL);

	/* FIXME: unnecessary with libgnomeui >= 2.16.0 */
	if (!been_here) {
		been_here = TRUE;
		gtk_about_dialog_set_url_hook (pk_notify_about_dialog_url_cb, NULL, NULL);
		gtk_about_dialog_set_email_hook (pk_notify_about_dialog_url_cb, "mailto:", NULL);
	}

	gtk_window_set_default_icon_name ("system-software-installer");
	gtk_show_about_dialog (NULL,
			       "version", VERSION,
			       "copyright", "Copyright \xc2\xa9 2007 Richard Hughes",
			       "license", license_trans,
			       "website-label", _("PackageKit Website"),
			       "website", "www.packagekit.org",
			       "comments", "PackageKit",
			       "authors", authors,
			       "documenters", documenters,
			       "translator-credits", translators,
			       "logo-icon-name", "system-software-installer",
			       NULL);
	g_free (license_trans);
}

/**
 * pk_notify_popup_menu_cb:
 *
 * Display the popup menu.
 **/
static void
pk_notify_popup_menu_cb (GtkStatusIcon *status_icon,
			 guint          button,
			 guint32        timestamp,
			 PkNotify      *icon)
{
	GtkMenu *menu = (GtkMenu*) gtk_menu_new ();
	GtkWidget *item;
	GtkWidget *image;

	pk_debug ("icon right clicked");

	/* Preferences */
	item = gtk_image_menu_item_new_with_mnemonic (_("_Preferences"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (pk_notify_show_preferences_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* Separator for HIG? */
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

#if 0
	/* No help yet */
	item = gtk_image_menu_item_new_with_mnemonic (_("_Help"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_HELP, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (pk_notify_show_help_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
#endif

	/* About */
	item = gtk_image_menu_item_new_with_mnemonic (_("_About"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_ABOUT, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (pk_notify_show_about_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* show the menu */
	gtk_widget_show_all (GTK_WIDGET (menu));
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			gtk_status_icon_position_menu, status_icon,
			button, timestamp);
	if (button == 0) {
		gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
	}
}

static gboolean pk_notify_check_for_updates_cb (PkNotify *notify);
static void pk_notify_refresh_cache_finished_cb (PkClient *client, PkExitEnum exit_code, guint runtime, PkNotify *notify);
static gboolean pk_notify_query_updates (PkNotify *notify);

/**
 * pk_notify_update_system_finished_cb:
 **/
static void
pk_notify_update_system_finished_cb (PkClient *client, PkExitEnum exit_code, guint runtime, PkNotify *notify)
{
	PkRestartEnum restart;
	guint i;
	guint length;
	PkPackageId *ident;
	PkPackageItem *item;
	GString *message_text;
	guint skipped_number = 0;
	const gchar *message;
	gboolean value;

	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));

	/* we failed, show the icon */
	if (exit_code != PK_EXIT_ENUM_SUCCESS) {
		pk_smart_icon_set_icon_name (notify->priv->sicon, NULL);
		/* we failed, so re-get the update list */
		pk_notify_query_updates (notify);
	}

	/* check we got some packages */
	length = pk_client_package_buffer_get_size (client);
	pk_debug ("length=%i", length);
	if (length == 0) {
		pk_debug ("no updates");
		return;
	}

	/* are we accepting notifications */
	value = gconf_client_get_bool (notify->priv->gconf_client, PK_CONF_NOTIFY_MESSAGE, NULL);
	if (value == FALSE) {
		pk_debug ("not showing notification as prevented in gconf");
		return;
	}

	message_text = g_string_new ("");

	/* find any we skipped */
	for (i=0; i<length; i++) {
		item = pk_client_package_buffer_get_item (client, i);
		pk_debug ("%s, %s, %s", pk_info_enum_to_text (item->info),
			  item->package_id, item->summary);
		ident = pk_package_id_new_from_string (item->package_id);
		if (item->info == PK_INFO_ENUM_BLOCKED) {
			skipped_number++;
			g_string_append_printf (message_text, "<b>%s</b> - %s\n",
						ident->name, item->summary);
		}
		pk_package_id_free (ident);
	}

	/* notify the user if there were skipped entries */
	if (skipped_number > 0) {
		message = ngettext (_("One package was skipped:\n"),
				    _("Some packages were skipped:\n"), skipped_number);
		g_string_prepend (message_text, message);
	}

	/* add a message that we need to restart */
	restart = pk_client_get_require_restart (client);
	if (restart != PK_RESTART_ENUM_NONE) {
		message = pk_restart_enum_to_localised_text (restart);

		/* add a gap if we are putting both */
		if (skipped_number > 0) {
			g_string_append (message_text, "\n");
		}

		g_string_append (message_text, message);
		g_string_append (message_text, "\n");
	}

	/* trim off extra newlines */
	if (message_text->len != 0) {
		g_string_set_size (message_text, message_text->len-1);
	}

	/* do the notify, and show the right buttons */
	pk_debug ("Doing notification");
	pk_smart_icon_notify_new (notify->priv->sicon,
				  _("The system update has completed"), message_text->str,
				  "software-update-available",
				  PK_NOTIFY_URGENCY_LOW, PK_NOTIFY_TIMEOUT_LONG);
	if (restart == PK_RESTART_ENUM_SYSTEM) {
		pk_smart_icon_notify_button (notify->priv->sicon, PK_NOTIFY_BUTTON_RESTART_COMPUTER, NULL);
	}
	pk_smart_icon_notify_button (notify->priv->sicon, PK_NOTIFY_BUTTON_DO_NOT_SHOW_AGAIN, PK_CONF_NOTIFY_RESTART);
	pk_smart_icon_notify_show (notify->priv->sicon);
	g_string_free (message_text, TRUE);
}

/**
 * pk_notify_update_system:
 **/
static gboolean
pk_notify_update_system (PkNotify *notify)
{
	gboolean ret;
	GError *error = NULL;
	gchar *message;

	g_return_val_if_fail (notify != NULL, FALSE);
	g_return_val_if_fail (PK_IS_NOTIFY (notify), FALSE);

	pk_debug ("install updates");
	ret = pk_client_update_system (notify->priv->client_update_system, &error);
	if (ret) {
		pk_smart_icon_set_icon_name (notify->priv->sicon, NULL);
	} else {
		pk_warning ("failed to update system: %s", error->message);
		message = g_strdup_printf (_("Client action was refused: %s"), error->message);
		g_error_free (error);
		pk_smart_icon_notify_new (notify->priv->sicon, _("Failed to update system"), message,
				      "process-stop", PK_NOTIFY_URGENCY_LOW, PK_NOTIFY_TIMEOUT_SHORT);
		g_free (message);
		pk_smart_icon_notify_button (notify->priv->sicon, PK_NOTIFY_BUTTON_DO_NOT_SHOW_AGAIN, PK_CONF_NOTIFY_ERROR);
		pk_smart_icon_notify_show (notify->priv->sicon);
	}
	return ret;
}

/**
 * pk_notify_menuitem_update_system_cb:
 **/
static void
pk_notify_menuitem_update_system_cb (GtkMenuItem *item, gpointer data)
{
	PkNotify *notify = PK_NOTIFY (data);
	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));
	pk_notify_update_system (notify);
}

/**
 * pk_notify_menuitem_show_updates_cb:
 **/
static void
pk_notify_menuitem_show_updates_cb (GtkMenuItem *item, gpointer data)
{
	const gchar *command = "pk-update-viewer";
	if (g_spawn_command_line_async (command, NULL) == FALSE) {
		pk_warning ("Couldn't execute command: %s", command);
	}
}

/**
 * pk_notify_activate_update_cb:
 * @button: Which buttons are pressed
 *
 * Callback when the icon is clicked
 **/
static void
pk_notify_activate_update_cb (GtkStatusIcon *status_icon,
			      PkNotify      *icon)
{
	GtkMenu *menu = (GtkMenu*) gtk_menu_new ();
	GtkWidget *item;
	GtkWidget *image;

	pk_debug ("icon left clicked");

	/* show updates */
	item = gtk_image_menu_item_new_with_mnemonic (_("_Show Updates"));
	image = gtk_image_new_from_icon_name ("system-software-update", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (pk_notify_menuitem_show_updates_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* update system */
	item = gtk_image_menu_item_new_with_mnemonic (_("_Update System Now"));
	image = gtk_image_new_from_icon_name ("software-update-available", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (pk_notify_menuitem_update_system_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* show the menu */
	gtk_widget_show_all (GTK_WIDGET (menu));
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			gtk_status_icon_position_menu, status_icon,
			1, gtk_get_current_event_time());
}

/**
 * pk_connection_changed_cb:
 **/
static void
pk_connection_changed_cb (PkConnection *pconnection, gboolean connected, PkNotify *notify)
{
	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));
	pk_debug ("connected=%i", connected);
}

/**
 * pk_notify_critical_updates_warning:
 **/
static void
pk_notify_critical_updates_warning (PkNotify *notify, const gchar *details, guint number)
{
	const gchar *title;
	gchar *message;
	gboolean value;

	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));

        /* are we accepting notifications */
        value = gconf_client_get_bool (notify->priv->gconf_client, PK_CONF_NOTIFY_CRITICAL, NULL);
        if (value == FALSE) {
                pk_debug ("not showing notification as prevented in gconf");
                return;
        }

	title = ngettext ("Security update available", "Security updates available", number);
	message = g_strdup_printf (ngettext ("The following important update is available for your computer:\n\n%s",
					     "The following important updates are available for your computer:\n\n%s", number), details);

	pk_debug ("Doing critical updates warning: %s", message);
	pk_smart_icon_notify_new (notify->priv->sicon, title, message, "software-update-urgent",
				  PK_NOTIFY_URGENCY_CRITICAL, PK_NOTIFY_TIMEOUT_NEVER);
	pk_smart_icon_notify_button (notify->priv->sicon, PK_NOTIFY_BUTTON_UPDATE_COMPUTER, NULL);
	pk_smart_icon_notify_button (notify->priv->sicon, PK_NOTIFY_BUTTON_DO_NOT_WARN_AGAIN, PK_CONF_NOTIFY_CRITICAL);
	pk_smart_icon_notify_show (notify->priv->sicon);

	g_free (message);
}

/**
 * pk_notify_auto_update_message:
 **/
static void
pk_notify_auto_update_message (PkNotify *notify)
{
	gboolean value;

	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));

	/* are we accepting notifications */
        value = gconf_client_get_bool (notify->priv->gconf_client, PK_CONF_NOTIFY_MESSAGE, NULL);
        if (value == FALSE) {
                pk_debug ("not showing notification as prevented in gconf");
                return;
        }

	pk_smart_icon_notify_new (notify->priv->sicon,
				  _("Updates are being installed"),
				  _("Updates are being automatically installed on your computer"), "software-update-urgent",
				  PK_NOTIFY_URGENCY_LOW, PK_NOTIFY_TIMEOUT_LONG);
	pk_smart_icon_notify_button (notify->priv->sicon, PK_NOTIFY_BUTTON_CANCEL_UPDATE, NULL);
	pk_smart_icon_notify_button (notify->priv->sicon, PK_NOTIFY_BUTTON_DO_NOT_SHOW_AGAIN, PK_CONF_NOTIFY_STARTED);
	pk_smart_icon_notify_show (notify->priv->sicon);
}

/**
 * pk_notify_client_packages_to_enum_list:
 **/
static PkEnumList *
pk_notify_client_packages_to_enum_list (PkNotify *notify, PkClient *client)
{
	guint i;
	guint length;
	PkEnumList *elist;
	PkPackageItem *item;

	g_return_val_if_fail (notify != NULL, NULL);
	g_return_val_if_fail (PK_IS_NOTIFY (notify), NULL);

	/* shortcut */
	length = pk_client_package_buffer_get_size (client);
	if (length == 0) {
		return NULL;
	}

	/* we can use an enumerated list */
	elist = pk_enum_list_new ();

	/* add each status to a list */
	for (i=0; i<length; i++) {
		item = pk_client_package_buffer_get_item (client, i);
		if (item == NULL) {
			pk_warning ("not found item %i", i);
			break;
		}
		pk_debug ("%s %s", item->package_id, pk_info_enum_to_text (item->info));
		pk_enum_list_append (elist, item->info);
	}
	return elist;
}

/**
 * pk_notify_get_best_update_icon:
 **/
static const gchar *
pk_notify_get_best_update_icon (PkNotify *notify, PkClient *client)
{
	gint value;
	PkEnumList *elist;
	const gchar *icon;

	g_return_val_if_fail (notify != NULL, NULL);
	g_return_val_if_fail (PK_IS_NOTIFY (notify), NULL);

	/* get an enumerated list with all the update types */
	elist = pk_notify_client_packages_to_enum_list (notify, client);

	/* get the most important icon */
	value = pk_enum_list_contains_priority (elist,
						PK_INFO_ENUM_SECURITY,
						PK_INFO_ENUM_IMPORTANT,
						PK_INFO_ENUM_BUGFIX,
						PK_INFO_ENUM_NORMAL,
						PK_INFO_ENUM_ENHANCEMENT,
						PK_INFO_ENUM_LOW, -1);
	if (value == -1) {
		pk_warning ("should not be possible!");
		value = PK_INFO_ENUM_LOW;
	}

	/* get the icon */
	icon = pk_info_enum_to_icon_name (value);

	g_object_unref (elist);
	return icon;
}

/**
 * pk_notify_check_on_battery:
 **/
static gboolean
pk_notify_check_on_battery (PkNotify *notify)
{
	gboolean on_battery;
	gboolean conf_update_battery;
	gboolean value;

	g_return_val_if_fail (notify != NULL, FALSE);
	g_return_val_if_fail (PK_IS_NOTIFY (notify), FALSE);

	on_battery = pk_auto_refresh_get_on_battery (notify->priv->arefresh);
	conf_update_battery = gconf_client_get_bool (notify->priv->gconf_client, PK_CONF_UPDATE_BATTERY, NULL);
	if (!conf_update_battery && on_battery) {
		/* are we accepting notifications */
		value = gconf_client_get_bool (notify->priv->gconf_client, PK_CONF_NOTIFY_BATTERY_UPDATE, NULL);
		if (value) {
			pk_smart_icon_notify_new (notify->priv->sicon,
						  _("Will not install updates"),
						  _("Automatic updates are not being installed as the computer is on battery power"),
					      "dialog-information", PK_NOTIFY_URGENCY_LOW, PK_NOTIFY_TIMEOUT_LONG);
			pk_smart_icon_notify_button (notify->priv->sicon,
						     PK_NOTIFY_BUTTON_DO_NOT_SHOW_AGAIN,
						     PK_CONF_NOTIFY_BATTERY_UPDATE);
			pk_smart_icon_notify_show (notify->priv->sicon);
		}
		return FALSE;
	}
	return TRUE;
}

/**
 * pk_notify_get_update_policy:
 **/
static PkUpdateEnum
pk_notify_get_update_policy (PkNotify *notify)
{
	PkUpdateEnum update;
	gchar *updates;

	g_return_val_if_fail (notify != NULL, FALSE);
	g_return_val_if_fail (PK_IS_NOTIFY (notify), FALSE);

	updates = gconf_client_get_string (notify->priv->gconf_client, PK_CONF_AUTO_UPDATE, NULL);
	if (updates == NULL) {
		pk_warning ("'%s' gconf key is null!", PK_CONF_AUTO_UPDATE);
		return PK_UPDATE_ENUM_UNKNOWN;
	}
	update = pk_update_enum_from_text (updates);
	g_free (updates);
	return update;
}

/**
 * pk_notify_query_updates_finished_cb:
 **/
static void
pk_notify_query_updates_finished_cb (PkClient *client, PkExitEnum exit, guint runtime, PkNotify *notify)
{
	PkPackageItem *item;
	guint length;
	guint i;
	gboolean ret;
	GString *status_security;
	GString *status_tooltip;
	PkUpdateEnum update;
	PkPackageId *ident;
	GPtrArray *security_array;

	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));

	status_security = g_string_new ("");
	status_tooltip = g_string_new ("");
	security_array = g_ptr_array_new ();

	/* find packages */
	length = pk_client_package_buffer_get_size (client);
	pk_debug ("length=%i", length);

	/* we have no updates */
	if (length == 0) {
		pk_debug ("no updates");
		pk_smart_icon_set_icon_name (notify->priv->sicon, NULL);
		goto out;
	}

	/* find the security updates */
	for (i=0; i<length; i++) {
		item = pk_client_package_buffer_get_item (client, i);
		pk_debug ("%s, %s, %s", pk_info_enum_to_text (item->info),
			  item->package_id, item->summary);
		ident = pk_package_id_new_from_string (item->package_id);
		if (item->info == PK_INFO_ENUM_SECURITY) {
			/* add to array */
			g_ptr_array_add (security_array, g_strdup (item->package_id));
			g_string_append_printf (status_security, "<b>%s</b> - %s\n",
						ident->name, item->summary);
		}
		pk_package_id_free (ident);
	}

	/* we are done querying this */
	g_object_unref (client);

	/* do we do the automatic updates? */
	update = pk_notify_get_update_policy (notify);
	if (update == PK_UPDATE_ENUM_UNKNOWN) {
		pk_warning ("policy unknown");
		goto out;
	}

	/* is policy none? */
	if (update == PK_UPDATE_ENUM_NONE) {
		const gchar *icon;
		pk_debug ("not updating as policy NONE");

		/* work out icon */
		icon = pk_notify_get_best_update_icon (notify, client);

		/* trim off extra newlines */
		if (status_security->len != 0) {
			g_string_set_size (status_security, status_security->len-1);
		}

		/* make tooltip */
		g_string_append_printf (status_tooltip, ngettext ("There is %d update pending",
								  "There are %d updates pending", length), length);

		pk_smart_icon_set_icon_name (notify->priv->sicon, icon);
		pk_smart_icon_set_tooltip (notify->priv->sicon, status_tooltip->str);

		/* do we warn the user? */
		if (security_array->len > 0) {
			pk_notify_critical_updates_warning (notify, status_security->str, length);
		}
		goto out;
	}

	/* are we on battery and configured to skip the action */
	ret = pk_notify_check_on_battery (notify);
	if (!ret) {
		pk_debug ("on battery so not doing update");
		goto out;
	}

	/* just do security updates */
	if (update == PK_UPDATE_ENUM_SECURITY && security_array->len > 0) {
		gchar **package_ids;
		gboolean ret;
		GError *error = NULL;

		pk_debug ("just process security updates");
		package_ids = pk_package_ids_from_array (security_array);
		ret = pk_client_update_packages_strv (notify->priv->client_update_system, package_ids, &error);
		if (!ret) {
			pk_warning ("Individual updates failed: %s", error->message);
			g_error_free (error);
		}
		g_strfreev (package_ids);
		goto out;
	}

	/* just do everything */
	if (update == PK_UPDATE_ENUM_ALL) {
		pk_debug ("we should do the update automatically!");
		ret = pk_notify_update_system (notify);
		if (ret) {
			pk_notify_auto_update_message (notify);
		} else {
			pk_warning ("update failed");
		}
		goto out;
	}

	/* shouldn't happen */
	pk_warning ("unknown update mode");
out:
	g_string_free (status_security, TRUE);
	g_string_free (status_tooltip, TRUE);

	/* get rid of the array, and free the contents */
	g_ptr_array_free (security_array, TRUE);
}

/**
 * pk_notify_error_code_cb:
 **/
static void
pk_notify_error_code_cb (PkClient *client, PkErrorCodeEnum error_code, const gchar *details, PkNotify *notify)
{
	const gchar *title;
	gboolean value;

	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));

	title = pk_error_enum_to_localised_text (error_code);

	/* ignore some errors */
	if (error_code == PK_ERROR_ENUM_PROCESS_KILL ||
	    error_code == PK_ERROR_ENUM_TRANSACTION_CANCELLED) {
		pk_debug ("error ignored %s\n%s", title, details);
		return;
	}

        /* are we accepting notifications */
        value = gconf_client_get_bool (notify->priv->gconf_client, PK_CONF_NOTIFY_ERROR, NULL);
        if (value == FALSE) {
                pk_debug ("not showing notification as prevented in gconf");
                return;
        }

	pk_smart_icon_notify_new (notify->priv->sicon, title, details, "help-browser",
				  PK_NOTIFY_URGENCY_LOW, PK_NOTIFY_TIMEOUT_LONG);
	pk_smart_icon_notify_button (notify->priv->sicon, PK_NOTIFY_BUTTON_DO_NOT_SHOW_AGAIN, PK_CONF_NOTIFY_ERROR);
	pk_smart_icon_notify_show (notify->priv->sicon);
}

/**
 * pk_notify_query_updates:
 **/
static gboolean
pk_notify_query_updates (PkNotify *notify)
{
	gboolean ret;
	GError *error = NULL;
	PkClient *client;

	g_return_val_if_fail (notify != NULL, FALSE);
	g_return_val_if_fail (PK_IS_NOTIFY (notify), FALSE);

	if (pk_task_list_contains_role (notify->priv->tlist, PK_ROLE_ENUM_UPDATE_SYSTEM)) {
		pk_debug ("Not checking for updates as already in progress");
		return FALSE;
	}

	client = pk_client_new ();
	g_signal_connect (client, "finished",
			  G_CALLBACK (pk_notify_query_updates_finished_cb), notify);
	g_signal_connect (client, "error-code",
			  G_CALLBACK (pk_notify_error_code_cb), notify);
	pk_client_set_use_buffer (client, TRUE, NULL);

	/* get updates */
	ret = pk_client_get_updates (client, "basename", &error);
	if (!ret) {
		pk_warning ("failed to get updates: %s", error->message);
		g_error_free (error);
	}
	return ret;
}

/**
 * pk_notify_refresh_cache_finished_cb:
 **/
static void
pk_notify_refresh_cache_finished_cb (PkClient *client, PkExitEnum exit_code, guint runtime, PkNotify *notify)
{
	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));

	pk_debug ("finished refreshing cache :%s", pk_exit_enum_to_text (exit_code));
	if (exit_code != PK_EXIT_ENUM_SUCCESS) {
		/* we failed to get the cache */
		notify->priv->cache_okay = FALSE;
	} else {
		/* stop the polling */
		notify->priv->cache_okay = TRUE;

		/* now try to get updates */
		pk_debug ("get updates");
		pk_notify_query_updates (notify);
	}
	notify->priv->cache_update_in_progress = FALSE;
	g_object_unref (client);
}

/**
 * pk_notify_check_for_updates_cb:
 **/
static gboolean
pk_notify_check_for_updates_cb (PkNotify *notify)
{
	gboolean ret;
	PkClient *client;
	pk_debug ("refresh cache");

	g_return_val_if_fail (notify != NULL, FALSE);
	g_return_val_if_fail (PK_IS_NOTIFY (notify), FALSE);

	/* got a cache, no need to poll */
	if (notify->priv->cache_okay) {
		return FALSE;
	}

	/* already in progress, but not yet certified okay */
	if (notify->priv->cache_update_in_progress) {
		return TRUE;
	}

	notify->priv->cache_update_in_progress = TRUE;
	notify->priv->cache_okay = TRUE;
	client = pk_client_new ();
	g_signal_connect (client, "finished",
			  G_CALLBACK (pk_notify_refresh_cache_finished_cb), notify);
	g_signal_connect (client, "error-code",
			  G_CALLBACK (pk_notify_error_code_cb), notify);
	ret = pk_client_refresh_cache (client, TRUE, NULL);
	if (ret == FALSE) {
		g_object_unref (client);
		pk_warning ("failed to refresh cache");
		/* try again in a few minutes */
	}
	return TRUE;
}

/**
 * pk_notify_updates_changed_cb:
 **/
static void
pk_notify_updates_changed_cb (PkClient *client, PkNotify *notify)
{
	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));

	/* now try to get newest update list */
	pk_debug ("get updates");
	pk_notify_query_updates (notify);
}

/**
 * pk_notify_task_list_changed_cb:
 **/
static void
pk_notify_task_list_changed_cb (PkTaskList *tlist, PkNotify *notify)
{
	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));
	/* hide icon if we are updating */
	if (pk_task_list_contains_role (tlist, PK_ROLE_ENUM_UPDATE_SYSTEM)) {
		pk_smart_icon_set_icon_name (notify->priv->sicon, NULL);
	}
}

/**
 * pk_notify_auto_refresh_cache_cb:
 **/
static void
pk_notify_auto_refresh_cache_cb (PkAutoRefresh *arefresh, PkNotify *notify)
{
	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));

	/* schedule another update */
	pk_notify_check_for_updates_cb (notify);
}

/**
 * pk_notify_auto_get_updates_cb:
 **/
static void
pk_notify_auto_get_updates_cb (PkAutoRefresh *arefresh, PkNotify *notify)
{
	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));

	/* show the icon at login time
	 * hopefully it just needs a quick network access, else we may have to
	 * make it a gconf variable */
	pk_notify_query_updates (notify);
}

/**
 * pk_notify_smart_icon_notify_button:
 **/
static void
pk_notify_smart_icon_notify_button (PkSmartIcon *sicon, PkNotifyButton button,
				    const gchar *data, PkNotify *notify)
{
	gboolean ret;

	g_return_if_fail (notify != NULL);
	g_return_if_fail (PK_IS_NOTIFY (notify));

	pk_debug ("got: %i with data %s", button, data);
	/* find the localised text */
	if (button == PK_NOTIFY_BUTTON_DO_NOT_SHOW_AGAIN ||
	    button == PK_NOTIFY_BUTTON_DO_NOT_WARN_AGAIN) {
		if (data == NULL) {
			pk_warning ("data NULL");
		} else {
			pk_debug ("setting %s to FALSE", data);
			gconf_client_set_bool (notify->priv->gconf_client, data, FALSE, NULL);
		}
	} else if (button == PK_NOTIFY_BUTTON_CANCEL_UPDATE) {
		gboolean ret;
		ret = pk_client_cancel (notify->priv->client_update_system, NULL);
		if (ret == FALSE) {
			pk_warning ("cancelling updates failed");
			pk_smart_icon_notify_new (notify->priv->sicon,
					      _("Could not stop"),
					      _("Could not cancel the system update"), "process-stop",
					      PK_NOTIFY_URGENCY_LOW, PK_NOTIFY_TIMEOUT_SHORT);
			pk_smart_icon_notify_show (notify->priv->sicon);
		}
	} else if (button == PK_NOTIFY_BUTTON_UPDATE_COMPUTER) {
		pk_notify_update_system (notify);
	} else if (button == PK_NOTIFY_BUTTON_RESTART_COMPUTER) {
		/* restart using gnome-power-manager */
		ret = pk_restart_system ();
		if (!ret) {
			pk_warning ("failed to reboot");
		}
	}
}

/**
 * pk_notify_init:
 * @notify: This class instance
 **/
static void
pk_notify_init (PkNotify *notify)
{
	GtkStatusIcon *status_icon;
	notify->priv = PK_NOTIFY_GET_PRIVATE (notify);

	notify->priv->sicon = pk_smart_icon_new ();
	g_signal_connect (notify->priv->sicon, "notification-button",
			  G_CALLBACK (pk_notify_smart_icon_notify_button), notify);

	notify->priv->gconf_client = gconf_client_get_default ();
	notify->priv->arefresh = pk_auto_refresh_new ();
	g_signal_connect (notify->priv->arefresh, "refresh-cache",
			  G_CALLBACK (pk_notify_auto_refresh_cache_cb), notify);
	g_signal_connect (notify->priv->arefresh, "get-updates",
			  G_CALLBACK (pk_notify_auto_get_updates_cb), notify);

	/* right click actions are common */
	status_icon = pk_smart_icon_get_status_icon (notify->priv->sicon);
	g_signal_connect_object (G_OBJECT (status_icon),
				 "popup_menu",
				 G_CALLBACK (pk_notify_popup_menu_cb),
				 notify, 0);
	g_signal_connect_object (G_OBJECT (status_icon),
				 "activate",
				 G_CALLBACK (pk_notify_activate_update_cb),
				 notify, 0);

	notify->priv->pconnection = pk_connection_new ();
	g_signal_connect (notify->priv->pconnection, "connection-changed",
			  G_CALLBACK (pk_connection_changed_cb), notify);
	if (pk_connection_valid (notify->priv->pconnection)) {
		pk_connection_changed_cb (notify->priv->pconnection, TRUE, notify);
	}

	/* use a client to get the updates-changed signal */
	notify->priv->client_update_system = pk_client_new ();
	pk_client_set_use_buffer (notify->priv->client_update_system, TRUE, NULL);
	g_signal_connect (notify->priv->client_update_system, "updates-changed",
			  G_CALLBACK (pk_notify_updates_changed_cb), notify);
	g_signal_connect (notify->priv->client_update_system, "finished",
			  G_CALLBACK (pk_notify_update_system_finished_cb), notify);
	g_signal_connect (notify->priv->client_update_system, "error-code",
			  G_CALLBACK (pk_notify_error_code_cb), notify);

	/* we need the task list so we can hide the update icon when we are doing the update */
	notify->priv->tlist = pk_task_list_new ();
	g_signal_connect (notify->priv->tlist, "task-list-changed",
			  G_CALLBACK (pk_notify_task_list_changed_cb), notify);

	/* refresh the cache, and poll until we get a good refresh */
	notify->priv->cache_okay = FALSE;
	notify->priv->cache_update_in_progress = FALSE;
}

/**
 * pk_notify_finalize:
 * @object: The object to finalize
 **/
static void
pk_notify_finalize (GObject *object)
{
	PkNotify *notify;

	g_return_if_fail (object != NULL);
	g_return_if_fail (PK_IS_NOTIFY (object));

	notify = PK_NOTIFY (object);

	g_return_if_fail (notify->priv != NULL);

	g_object_unref (notify->priv->sicon);
	g_object_unref (notify->priv->pconnection);
	g_object_unref (notify->priv->client_update_system);
	g_object_unref (notify->priv->tlist);
	g_object_unref (notify->priv->arefresh);
	g_object_unref (notify->priv->gconf_client);

	G_OBJECT_CLASS (pk_notify_parent_class)->finalize (object);
}

/**
 * pk_notify_new:
 *
 * Return value: a new PkNotify object.
 **/
PkNotify *
pk_notify_new (void)
{
	PkNotify *notify;
	notify = g_object_new (PK_TYPE_NOTIFY, NULL);
	return PK_NOTIFY (notify);
}
