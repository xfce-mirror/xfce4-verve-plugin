/***************************************************************************
 *            verve-dbus-service.c
 *
 *  $Id: verve-env.h 1022 2006-02-08 16:07:09Z jpohlmann $
 *  Copyright  2006  Jannis Pohlmann
 *  info@sten-net.de
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>

#include "verve-dbus-service.h"

#define VERVE_DBUS_SERVICE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), VERVE_TYPE_DBUS_SERVICE, VerveDBusServicePrivate))

/* Property identifiers */
enum
{
  PROP_0,
  PROP_VERVE_PLUGIN,
};

static void verve_dbus_service_class_init (VerveDBusServiceClass *klass);
static void verve_dbus_service_init (VerveDBusService *dbus_service);
static void verve_dbus_service_finalize (GObject *object);
static void verve_dbus_service_get_property (GObject *object,
                                             guint prop_id,
                                             GValue *value,
                                             GParamSpec *pspec);
static void verve_dbus_service_set_property (GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static gboolean verve_dbus_service_open_dialog (VerveDBusService *dbus_service, 
                                                const gchar *dir, 
                                                const gchar *display,
                                                GError **error);

struct _VerveDBusServiceClass
{
  GObjectClass __parent__;
};

struct _VerveDBusService
{
  GObject __parent__;

  DBusGConnection *connection;
  GtkWidget *input;
};

static GObjectClass *verve_dbus_service_parent_class;

GType
verve_dbus_service_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
  {
    static const GTypeInfo info = 
    {
      sizeof (VerveDBusServiceClass),
      NULL,
      NULL,
      (GClassInitFunc) verve_dbus_service_class_init,
      NULL,
      NULL,
      sizeof (VerveDBusService),
      0,
      (GInstanceInitFunc) verve_dbus_service_init,
      NULL,
    };

    type = g_type_register_static (G_TYPE_OBJECT, "VerveDBusService", &info, 0);
  }
  
  return type;
}

static void
verve_dbus_service_class_init (VerveDBusServiceClass *klass)
{
  extern const DBusGObjectInfo dbus_glib_verve_dbus_service_object_info;
  GObjectClass *gobject_class;

  // g_type_class_add_private (klass, sizeof (VerveDBusServicePrivate));
  
  verve_dbus_service_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = verve_dbus_service_finalize;
  gobject_class->get_property = verve_dbus_service_get_property;
  gobject_class->set_property = verve_dbus_service_set_property;

  /* Install verve plugin property */
  g_object_class_install_property (gobject_class, 
                                   PROP_VERVE_PLUGIN,
                                   g_param_spec_pointer ("input",
                                                         "input",
                                                         "input",
                                                         G_PARAM_READABLE|G_PARAM_WRITABLE));

  /* Install the D-BUS info */
  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass), &dbus_glib_verve_dbus_service_object_info);
}

static void
verve_dbus_service_init (VerveDBusService *dbus_service)
{
  GError *error = NULL;

  /* Try to connect to the session bus */
  dbus_service->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (G_LIKELY (dbus_service->connection != NULL))
  {
    /* Register the /org/xfce/RunDialog object for Verve */
    dbus_g_connection_register_g_object (dbus_service->connection, "/org/xfce/RunDialog", G_OBJECT (dbus_service));

    /* Request the org.xfce.Verve name for Verve */
    dbus_bus_request_name (dbus_g_connection_get_connection (dbus_service->connection), "org.xfce.Verve", DBUS_NAME_FLAG_REPLACE_EXISTING, NULL);

    /* Request the org.xfce.RunDialog name for Verve */
    dbus_bus_request_name (dbus_g_connection_get_connection (dbus_service->connection), "org.xfce.RunDialog", DBUS_NAME_FLAG_REPLACE_EXISTING, NULL);
  }
  else
  {
    /* Notify the user that D-BUS won't be a available */
    g_fprintf (stderr, "Verve: Failed to connect to the D-BUS session bus: %s\n", error->message);
    g_error_free (error);
  }
}

static void
verve_dbus_service_finalize (GObject *object)
{
  VerveDBusService *dbus_service = VERVE_DBUS_SERVICE (object);

  /* Release the D-BUS connection object */
  if (G_LIKELY (dbus_service->connection != NULL))
    dbus_g_connection_unref (dbus_service->connection);

  (*G_OBJECT_CLASS (verve_dbus_service_parent_class)->finalize) (object);
}

static void
verve_dbus_service_get_property (GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  switch (prop_id)
  {
    case PROP_VERVE_PLUGIN:
      g_value_set_pointer (value, (VERVE_DBUS_SERVICE (object))->input);
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
verve_dbus_service_set_property (GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
  g_printf("%s, %s\n", value, g_value_get_pointer (value));
  VerveDBusService *dbus_service = VERVE_DBUS_SERVICE (object);
  g_printf("%s, %s\n", value, g_value_get_pointer (value));

  switch (prop_id)
  {
    case PROP_VERVE_PLUGIN:
      g_printf("%s, %s\n", value, g_value_get_pointer (value));
      dbus_service->input = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
verve_dbus_service_open_dialog (VerveDBusService *dbus_service, 
                                const gchar *dir, 
                                const gchar *display, 
                                GError **error)
{
  gtk_entry_set_text (GTK_ENTRY (dbus_service->input), "test");
  gtk_widget_grab_focus (GTK_WIDGET (dbus_service->input));
  
  return TRUE;
}
  
#include "verve-dbus-service-infos.h"

/* vim:set expandtab ts=1 sw=2: */
