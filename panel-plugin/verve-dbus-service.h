/***************************************************************************
 *            verve-dbus-service.h
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
 
#ifndef __VERVE_DBUS_SERVICE_H__
#define __VERVE_DBUS_SERVICE_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _VerveDBusServiceClass VerveDBusServiceClass;
typedef struct _VerveDBusService VerveDBusService;
  
#define VERVE_TYPE_DBUS_SERVICE            (verve_dbus_service_get_type ())
#define VERVE_DBUS_SERVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VERVE_TYPE_DBUS_SERVICE, VerveDBusService))
#define VERVE_DBUS_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass, VERVE_TYPE_DBUS_SERVICE, VerveDBusServiceClass)))
#define VERVE_IS_DBUS_SERVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VERVE_TYPE_DBUS_SERVICE))
#define VERVE_IS_DBUS_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VERVE_TYPE_DBUS_SERVICE))
#define VERVE_DBUS_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VERVE_TYPE_DBUS_SERVICE, VerveDBusServiceClass))

GType verve_dbus_service_get_type (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* __VERVE_DBUS_SERVICE_H__ */

/* vim:set expandtab ts=1 sw=2: */
