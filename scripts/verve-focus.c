/* vi:set expandtab sw=2 sts=2: */
/*-
 * Copyright (c) 2006-2007 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <glib.h>

#include <dbus/dbus.h>



int 
main (int     argc,
      gchar **argv)
{
  DBusConnection *connection;
  DBusMessage    *method;
  DBusMessage    *result;
  DBusError       error;
  gint            exit_status;

  /* Configure gettext */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* Initialize error structure */
  dbus_error_init (&error);

  /* Connect to the session bus */
  connection = dbus_bus_get (DBUS_BUS_SESSION, &error);

  /* Print error if connection failed */
  if (G_UNLIKELY (connection == NULL))
    {
      /* Print error message */
      g_error (_("Failed to connect to the D-BUS session bus."));

      /* Free error structure */
      dbus_error_free (&error);

      /* Exit with error code */
      return EXIT_FAILURE;
    }

  /* Generate D-BUS message */
  method = dbus_message_new_method_call ("org.xfce.Verve", "/org/xfce/RunDialog", "org.xfce.Verve", "GrabFocus");

  /* Send message and wait for reply */
  result = dbus_connection_send_with_reply_and_block (connection, method, 5000, &error);

  /* Destroy sent message */
  if (G_LIKELY (method != NULL))
    dbus_message_unref (method);

  /* Handle method send error */
  if (G_UNLIKELY (result == NULL))
    {
      /* Print error message */
      g_debug (_("There seems to be no Verve D-BUS provider (e.g. the Verve panel plugin) running."));

      /* Free error structure */
      dbus_error_free (&error);

      /* Set exit status */
      exit_status = EXIT_FAILURE;
    }
  else
    exit_status = EXIT_SUCCESS;

  /* Destroy result */
  if (G_LIKELY (result != NULL))
    dbus_message_unref (result);

  /* Disconnect from session bus */
  if (G_LIKELY (connection != NULL))
    dbus_connection_unref (connection);

  return exit_status;
}
