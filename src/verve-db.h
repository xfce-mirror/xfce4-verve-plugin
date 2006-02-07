/***************************************************************************
 *            verve-db.h
 *
 *  $Id$
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
 
#ifndef __VERVE_DB_H__
#define __VERVE_DB_H__

#include <glib-object.h>
#include <libxfce4util/libxfce4util.h>

enum 
{
  VERVE_COMMAND_TYPE_SIMPLE = 0,
  VERVE_COMMAND_TYPE_SCRIPT = 1,
};

typedef struct
{
  const gchar *name;
  const gchar *command;
} VerveSimpleCommand;

typedef struct
{
  const gchar *shell;
  const gchar *name;
  const gchar *target_filename;
} VerveScriptCommand;

G_BEGIN_DECLS;

typedef struct _VerveDbClass VerveDbClass;
typedef struct _VerveDb VerveDb;
  
#define VERVE_TYPE_DB            (verve_db_get_type ())
#define VERVE_DB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VERVE_TYPE_DB, VerveDb))
#define VERVE_DB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass, VERVE_TYPE_DB, VerveDbClass)))
#define VERVE_IS_DB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VERVE_TYPE_DB))
#define VERVE_IS_DB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VERVE_TYPE_DB))
#define VERVE_DB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VERVE_TYPE_DB, VerveDbClass))

GType verve_db_get_type (void) G_GNUC_CONST;
VerveDb *verve_db_get (void);
gboolean verve_db_has_command (VerveDb *db, const gchar *name);
gboolean verve_db_has_simple_command (VerveDb *db, const gchar *name);
gboolean verve_db_has_script_command (VerveDb *db, const gchar *name);
VerveSimpleCommand *verve_db_get_simple_command (VerveDb *db, const gchar *name);
VerveScriptCommand *verve_db_get_script_command (VerveDb *db, const gchar *name);
gboolean verve_db_exec_command (VerveDb *db, const gchar *name);
gboolean verve_db_exec_simple_command (VerveDb *db, const VerveSimpleCommand *cmd);
gboolean verve_db_exec_script_command (VerveDb *db, const VerveScriptCommand *cmd);
GList *verve_db_get_command_names (VerveDb *db);

void _verve_db_shutdown (void);

G_END_DECLS;

#endif /* __VERVE_DB_H__ */

/* vim:set expandtab ts=1 sw=2: */
