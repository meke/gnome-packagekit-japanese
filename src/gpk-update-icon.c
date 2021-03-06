/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2009 Richard Hughes <richard@hughsie.com>
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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <libnotify/notify.h>
#include <packagekit-glib2/packagekit.h>
#include <unique/unique.h>

#include "egg-debug.h"
#include "egg-dbus-monitor.h"

#include "gpk-check-update.h"
#include "gpk-watch.h"
#include "gpk-firmware.h"
#include "gpk-hardware.h"
#include "gpk-common.h"

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	gboolean program_version = FALSE;
	gboolean timed_exit = FALSE;
	GpkCheckUpdate *cupdate = NULL;
	GpkWatch *watch = NULL;
	GpkFirmware *firmware = NULL;
	GpkHardware *hardware = NULL;
	GOptionContext *context;
	UniqueApp *unique_app;
	gboolean ret;

	const GOptionEntry options[] = {
		{ "timed-exit", '\0', 0, G_OPTION_ARG_NONE, &timed_exit,
		  _("Exit after a small delay"), NULL },
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &program_version,
		  _("Show the program version and exit"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	if (! g_thread_supported ())
		g_thread_init (NULL);
	g_type_init ();
	gtk_init (&argc, &argv);
	dbus_g_thread_init ();
	notify_init ("gpk-update-icon");

	/* TRANSLATORS: program name, a session wide daemon to watch for updates and changing system state */
	g_set_application_name (_("Update Applet"));
	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, _("Update Applet"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	if (program_version) {
		g_print (VERSION "\n");
		return 0;
	}

	/* TRANSLATORS: title to pass to to the user if there are not enough privs */
	ret = gpk_check_privileged_user (_("Update applet"), FALSE);
	if (!ret) {
		egg_warning ("Exit: gpk_check_privileged_user returned FALSE");
		return 1;
	}

	/* are we already activated? */
	unique_app = unique_app_new ("org.freedesktop.PackageKit.UpdateIcon", NULL);
	if (unique_app_is_running (unique_app)) {
		egg_debug ("You have another instance running. This program will now close");
		unique_app_send_message (unique_app, UNIQUE_ACTIVATE, NULL);
		goto unique_out;
	}

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   GPK_DATA G_DIR_SEPARATOR_S "icons");

	/* create new objects */
	cupdate = gpk_check_update_new ();
	watch = gpk_watch_new ();
	firmware = gpk_firmware_new ();
	hardware = gpk_hardware_new ();

	/* Only timeout if we have specified iton the command line */
	if (timed_exit)
		g_timeout_add_seconds (120, (GSourceFunc) gtk_main_quit, NULL);

	/* wait */
	gtk_main ();

	g_object_unref (cupdate);
	g_object_unref (watch);
	g_object_unref (firmware);
	g_object_unref (hardware);
unique_out:
	g_object_unref (unique_app);
	return 0;
}

