/***************************************************************************
 *            verve-dbus-service.c
 *
 *  Copyright  2006-2007  Jannis Pohlmann
 *  jannis@xfce.org
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

#include <libxfce4panel/xfce-panel-plugin.h>

#include "verve-dbus-service.h"



static void     verve_dbus_service_class_init  (VerveDBusServiceClass *klass);
static void     verve_dbus_service_init        (VerveDBusService      *dbus_service);
static void     verve_dbus_service_finalize    (GObject               *object);
static gboolean verve_dbus_service_open_dialog (VerveDBusService      *dbus_service, 
                                                const gchar           *dir, 
                                                const gchar           *display,
                                                GError               **error);
static gboolean verve_dbus_service_grab_focus  (VerveDBusService      *dbus_service);



struct _VerveDBusServiceClass
{
  GObjectClass __parent__;

  /* Signal identifiers */
  guint open_dialog_signal_id;
  guint grab_focus_signal_id;
};

struct _VerveDBusService
{
  GObject __parent__;

  /* D-BUS connection */
  DBusGConnection *connection;
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

#if 0
  g_type_class_add_private (klass, sizeof (VerveDBusServicePrivate)); 
#endif
  
  verve_dbus_service_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = verve_dbus_service_finalize;

  /* Register "open-dialog" signal */
  klass->open_dialog_signal_id = g_signal_newv ("open-dialog",
                                                G_TYPE_FROM_CLASS (gobject_class),
                                                G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                                                NULL,
                                                NULL,
                                                NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE,
                                                0,
                                                NULL);

  /* Register "grab-focus" signal */
  klass->grab_focus_signal_id = g_signal_newv ("grab-focus",
                                               G_TYPE_FROM_CLASS (gobject_class),
                                               G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                                               NULL,
                                               NULL,
                                               NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE,
                                               0,
                                               NULL);

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

    /* Request service names */
#ifdef HAVE_DBUS_NEW_FLAGS
    dbus_bus_request_name (dbus_g_connection_get_connection (dbus_service->connection), "org.xfce.Verve", DBUS_NAME_FLAG_REPLACE_EXISTING|DBUS_NAME_FLAG_ALLOW_REPLACEMENT, NULL);
    dbus_bus_request_name (dbus_g_connection_get_connection (dbus_service->connection), "org.xfce.RunDialog", DBUS_NAME_FLAG_REPLACE_EXISTING|DBUS_NAME_FLAG_ALLOW_REPLACEMENT, NULL);
#else
    dbus_bus_request_name (dbus_g_connection_get_connection (dbus_service->connection), "org.xfce.Verve", DBUS_NAME_FLAG_REPLACE_EXISTING, NULL);
    dbus_bus_request_name (dbus_g_connection_get_connection (dbus_service->connection), "org.xfce.RunDialog", DBUS_NAME_FLAG_REPLACE_EXISTING, NULL);
#endif /* !HAVE_DBUS_NEW_FLAGS */
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



static gboolean
verve_dbus_service_open_dialog (VerveDBusService *dbus_service, 
                                const gchar *dir, 
                                const gchar *display, 
                                GError **error)
{
  /* Emit "open-dialog" signal */
  g_signal_emit (dbus_service, VERVE_DBUS_SERVICE_GET_CLASS (dbus_service)->open_dialog_signal_id, 0, NULL);

  return TRUE;
}



static gboolean
verve_dbus_service_grab_focus (VerveDBusService *dbus_service)
{
  /* Emit "grab-focus" signal */
  g_signal_emit (dbus_service, VERVE_DBUS_SERVICE_GET_CLASS (dbus_service)->grab_focus_signal_id, 0, NULL);

  return TRUE;
}
  


#include "verve-dbus-service-infos.h"

/* vim:set expandtab ts=1 sw=2: */
