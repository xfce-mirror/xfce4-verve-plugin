/***************************************************************************
 *            verve-db.c
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
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <malloc.h>
#include "verve.h"
#include "verve-db.h"
#include "verve-env.h"

/* GObject class methods */
static void verve_db_class_init (VerveDbClass *klass);
static void verve_db_init (VerveDb *db);
static void verve_db_finalize (GObject *object);

/* Internal functions */
void _verve_db_load_rc (VerveDb *db, const gchar *filename);
void _verve_db_load_script_rc (VerveDb *db, const XfceRc *rc);
void _verve_db_load_simple_rc (VerveDb *db, XfceRc *rc);
gboolean _verve_db_simple_command_close_rc (gpointer key, gpointer value, gpointer user_data);
gboolean _verve_db_script_command_close (gpointer key, gpointer value, gpointer user_data);
void _verve_db_simple_command_add_to_list (gpointer key, gpointer value, GList **list);
void _verve_db_script_command_add_to_list (gpointer key, gpointer value, GList **list);


/*********************************************************************
 * 
 * Verve Command Database
 * ----------------------
 *
 * This database manages all available virtual commands created by
 * the user.
 *
 *********************************************************************/

/* VerveDb class */
struct _VerveDbClass
{
  GObjectClass __parent__;
};

/* VerveDb instance definition */
struct _VerveDb
{
  GObject __parent__;
  
  /*< private >*/
  GHashTable *simple_commands;
  GHashTable *script_commands;
};

static GObjectClass *verve_db_parent_class;

/* GObject: Get VerveDb type */
GType
verve_db_get_type (void)
{
  static GType type = G_TYPE_INVALID;
  
  if (G_UNLIKELY (type == G_TYPE_INVALID))
  {
    static const GTypeInfo info =
    {
      sizeof (VerveDbClass),
      NULL,
      NULL,
      (GClassInitFunc) verve_db_class_init,
      NULL,
      NULL,
      sizeof (VerveDb),
      0,
      (GInstanceInitFunc) verve_db_init,
      NULL,
    };
    
    type = g_type_register_static (G_TYPE_OBJECT, "VerveDb", &info, 0);
  }
    
  return type;
}

/* GObject: Init VerveDb class */
static void verve_db_class_init (VerveDbClass *klass)
{
  GObjectClass *gobject_class;
  
  verve_db_parent_class = g_type_class_peek_parent (klass);
  
  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = verve_db_finalize;
}

/* GObject init a VerveDb instance */
static void 
verve_db_init (VerveDb *db)
{
  /* Init hash tables */
  db->simple_commands = g_hash_table_new (g_str_hash, g_str_equal);
  db->script_commands = g_hash_table_new (g_str_hash, g_str_equal);

  /* Create resource path pattern for .desktop files */
  gchar *filename_pattern = g_build_filename (g_get_application_name (), "*.desktop", NULL);
  
  /* Determine available .desktop files */
  gchar **filenames = xfce_resource_match (XFCE_RESOURCE_CONFIG, filename_pattern, TRUE);
  
  g_free (filename_pattern);
  
  if (G_LIKELY (filenames != NULL))
  {
    int i = 0;
    
    /* Iterate over filenames */
    gchar *filename = filenames[i];
    while (G_LIKELY (filename != NULL))
    {
      /* Load command structure */
      _verve_db_load_rc (db, filename);
      
      /* Step over to the next filename */
      filename = filenames[++i];
    }
  }
  
  /** Free .desktop filenames */
  g_strfreev (filenames);
}

/* GObject: Finalize (destroy) members of a VerveDb object */
static void 
verve_db_finalize (GObject *object)
{
  VerveDb *db = VERVE_DB (object);
  
  /* Destroy simple commands */
  g_hash_table_foreach_remove (db->simple_commands, _verve_db_simple_command_close_rc, NULL);
  g_hash_table_destroy (db->simple_commands);
  
  /* Destroy script commands */
  g_hash_table_foreach_remove (db->script_commands, _verve_db_script_command_close, NULL);
  g_hash_table_destroy (db->script_commands);
}

/* Create static VerveDb instance and return a pointer to it */
VerveDb*
verve_db_get (void)
{
  static VerveDb *db = NULL;
  
  if (G_UNLIKELY (db == NULL))
  {
    db = g_object_new (VERVE_TYPE_DB, NULL);
    g_object_add_weak_pointer (G_OBJECT (db), (gpointer) &db);
  }
  else 
  {
    g_object_ref (G_OBJECT (db));
  }
    
  return db;
}

/* Check if a command with this name exists */
gboolean 
verve_db_has_command (VerveDb *db, const gchar *name)
{
  gboolean result = verve_db_has_simple_command (db, name);
  return result || verve_db_has_script_command (db, name);
}

/* Check if a simple command with this name exists */ 
gboolean 
verve_db_has_simple_command (VerveDb *db, const gchar *name)
{
  return g_hash_table_lookup (db->simple_commands, name) != NULL;
}

/* Check if a script command with this name exists */
gboolean 
verve_db_has_script_command (VerveDb *db, const gchar *name)
{
  return g_hash_table_lookup (db->script_commands, name) != NULL;
}

/* Get simple command with this name */
VerveSimpleCommand*
verve_db_get_simple_command (VerveDb *db, const gchar *name)
{
  if (G_UNLIKELY (!verve_db_has_simple_command (db, name)))
    return NULL;
  return (VerveSimpleCommand *)g_hash_table_lookup (db->simple_commands, name);
}

/* Get script command with this name */
VerveScriptCommand*
verve_db_get_script_command (VerveDb *db, const gchar *name)
{
  if (G_UNLIKELY (!verve_db_has_script_command (db, name)))
    return NULL;
  return (VerveScriptCommand *)g_hash_table_lookup (db->script_commands, name);
}

/* Create and return a list containing all command names */
GList *
verve_db_get_command_names (VerveDb *db)
{
  GList *list = NULL;
  g_hash_table_foreach (db->simple_commands, (GHFunc)_verve_db_simple_command_add_to_list, 
                        (gpointer)&list);
  g_hash_table_foreach (db->script_commands, (GHFunc)_verve_db_script_command_add_to_list, 
                        (gpointer)&list);
  return list;
}

/* Execute command with this name */
gboolean 
verve_db_exec_command (VerveDb *db, const gchar *name)
{
  if (verve_db_has_simple_command (db, name))
  {
    VerveSimpleCommand *cmd = verve_db_get_simple_command (db, name);
    return _verve_db_exec_simple_command (db, cmd);
  }
  else if (verve_db_has_script_command (db, name))
  {
    VerveScriptCommand *cmd = verve_db_get_script_command (db, name);
    return _verve_db_exec_script_command (db, cmd);
  }
  
  return FALSE;
}

/* Internal: Shutdown Verve Command Database and free global objects */
void
_verve_db_shutdown (void)
{
  VerveDb *db = verve_db_get ();

  if (G_LIKELY (db != NULL))
  {
    g_object_unref (G_OBJECT (db));
  }
}

/* Internal: Load command config file */
void
_verve_db_load_rc (VerveDb *db, const gchar *filename)
{
  XfceRc *rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, filename, FALSE);
  
  g_return_if_fail (xfce_rc_has_entry (rc, "Type"));

  /* Get command type */
  gint cmd_type = xfce_rc_read_int_entry (rc, "Type", VERVE_COMMAND_TYPE_SIMPLE);
  
  switch (cmd_type)
  {
    case VERVE_COMMAND_TYPE_SCRIPT:
      _verve_db_load_script_rc (db, rc);
      break;
    
    case VERVE_COMMAND_TYPE_SIMPLE:
      _verve_db_load_simple_rc (db, rc);
      break;
  }

  xfce_rc_close (rc);
}
    
/* Internal: Load script (enhanced) command from config */
void 
_verve_db_load_script_rc (VerveDb *db, const XfceRc *rc)
{
  g_return_if_fail (xfce_rc_has_entry (rc, "Target"));
  g_return_if_fail (xfce_rc_has_entry (rc, "Shell"));
    
  /* Get basename of target */
  const gchar *basename = xfce_rc_read_entry (rc, "Target", NULL);
    
  /* Get resource path */
  gchar *resource = g_build_filename (g_get_application_name (), basename, NULL);
    
  /* Get absolute filename */
  gchar *filename = xfce_resource_lookup (XFCE_RESOURCE_DATA, resource);
    
  VerveScriptCommand *cmd;
  cmd = (VerveScriptCommand *)malloc (sizeof (VerveScriptCommand));
  cmd->name = g_strdup (xfce_rc_read_entry (rc, "Name", NULL));
  cmd->shell = g_strdup (xfce_rc_read_entry (rc, "Shell", "sh"));
  cmd->target_filename = g_strdup (filename);
      
  g_hash_table_insert (db->script_commands, (gpointer) cmd->name, (gpointer) cmd);
    
  g_free (resource);
  g_free (filename);
}

/* Internal: Close script command config */
gboolean 
_verve_db_script_command_close (gpointer key, gpointer value, gpointer user_data)
{
  VerveScriptCommand *cmd = (VerveScriptCommand *)value;
  return TRUE;
}

/* Load simple ('one line') command from config */
void
_verve_db_load_simple_rc (VerveDb *db, XfceRc *rc)
{
  g_return_if_fail (xfce_rc_has_entry (rc, "Command"));
    
  VerveSimpleCommand *cmd;
  cmd = (VerveSimpleCommand *)malloc (sizeof (VerveSimpleCommand));
  cmd->name = xfce_rc_read_entry (rc, "Name", NULL);
  cmd->command = xfce_rc_read_entry (rc, "Command", "");

  g_hash_table_insert (db->simple_commands, (gpointer) cmd->name, (gpointer) cmd);
}

/* Internal: Close simple command config */
gboolean 
_verve_db_simple_command_close_rc (gpointer key, gpointer value, gpointer user_data)
{
  VerveSimpleCommand *cmd = (VerveSimpleCommand *)value;
  return TRUE;
}

/* Internal: Execute simple command */
gboolean
_verve_db_exec_simple_command (VerveDb *db, const VerveSimpleCommand *cmd)
{
  return verve_spawn_command_line (cmd->command);
}

/* Internal: Execute script command */
gboolean
_verve_db_exec_script_command (VerveDb *db, const VerveScriptCommand *cmd)
{
  VerveEnv *env = verve_env_get ();
  const gchar *shell_path = verve_env_shell_get_path (env, cmd->shell);
  
  GString *command = g_string_new (cmd->target_filename);
  g_string_prepend (command, " ");
  g_string_prepend (command, shell_path);
  
  gboolean success = verve_spawn_command_line (command->str);
  
  g_string_free (command, TRUE);
  
  return success;
}
  
/* Internal: Add simple command to a GList */
void 
_verve_db_simple_command_add_to_list (gpointer key, gpointer value, GList **list)
{
  *list = g_list_append (*list, key);
}

/* Internal: Add script command to a GList */
void 
_verve_db_script_command_add_to_list (gpointer key, gpointer value, GList **list)
{
  *list = g_list_append (*list, key);
}

/* vim:set expandtab ts=1 sw=2: */
