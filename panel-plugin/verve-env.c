/***************************************************************************
 *            verve-env.c
 *
 *  $Id: verve-env.c 1050 2006-02-12 11:27:39Z jpohlmann $
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

#include "verve-env.h"

static void verve_env_class_init (VerveEnvClass *klass);
static void verve_env_init (VerveEnv *env);
static void verve_env_finalize (GObject *object);
static void verve_env_shells_init (VerveEnv *env);
void verve_env_shell_add_name_to_list (gpointer key, gpointer val, GList **list);
void verve_env_shell_add_path_to_list (gpointer key, gpointer val, GList **list);

struct _VerveEnvClass
{
  GObjectClass __parent__;
};

struct _VerveEnv
{
  GObject __parent__;
  
  /*< private >*/
  GHashTable *shells;
};

static GObjectClass *verve_env_parent_class;

GType
verve_env_get_type (void)
{
  static GType type = G_TYPE_INVALID;
  
  if (G_UNLIKELY (type == G_TYPE_INVALID))
  {
    static const GTypeInfo info =
    {
      sizeof (VerveEnvClass),
      NULL,
      NULL,
      (GClassInitFunc) verve_env_class_init,
      NULL,
      NULL,
      sizeof (VerveEnv),
      0,
      (GInstanceInitFunc) verve_env_init,
      NULL,
    };
    
    type = g_type_register_static (G_TYPE_OBJECT, "VerveEnv", &info, 0);
  }
    
  return type;
}

void
verve_env_class_init (VerveEnvClass *klass)
{
  GObjectClass *gobject_class;
  
  verve_env_parent_class = g_type_class_peek_parent (klass);
  
  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = verve_env_finalize;
}

void 
verve_env_init (VerveEnv *env)
{
  verve_env_shells_init (env);
}

static VerveEnv *env = NULL;  

VerveEnv 
*verve_env_get (void)
{
  if (G_UNLIKELY (env == NULL))
  {
    env = g_object_new (VERVE_TYPE_ENV, NULL);
    g_object_add_weak_pointer (G_OBJECT (env), (gpointer) &env);
  }
  else
    g_object_ref (G_OBJECT (env));
  
  return env;
}

void 
_verve_env_shutdown (void)
{
  if (G_LIKELY (env != NULL))
    g_object_unref (G_OBJECT (env));
}

static void 
verve_env_finalize (GObject *object)
{
  VerveEnv *env = VERVE_ENV (object);
  
  /* Destroy shell hash table */
  g_hash_table_destroy (env->shells);
}

const gchar *
verve_env_get_shell (void)
{
  const gchar *shell = g_getenv ("SHELL");
  if (G_UNLIKELY (shell == NULL))
    shell = "/bin/sh";
  return shell;
}

/*********************************************************************
 * 
 * Shell substitution and search. Supported shells/interpreters are 
 * Bash, sh and various scripting languages like Ruby and Python.
 *
 *********************************************************************/ 

static gchar *supported_shells[] = {
  /* Ruby variants: */    "ruby", "ruby1.8", "ruby1.6",               
  /* Shells: */           "sh", "bash", "fish", "zsh", "csh",
  /* Python variants: */  "python", "python2.3", "python2.4",
  /* PHP variants: */     "php", "php4", "php5",
};

void
verve_env_shells_init (VerveEnv *env)
{
  /* Init shell hash table */
  env->shells = g_hash_table_new (g_str_hash, g_str_equal);
  
  /* Search shells in PATH and add existing ones to the hash table */
  int i;
  for (i=0; i<G_N_ELEMENTS (supported_shells); i++)
  {
    gchar *shell_path = g_find_program_in_path (supported_shells[i]);
    if (G_LIKELY (shell_path != NULL))
    {
      g_hash_table_insert (env->shells, (gpointer) supported_shells[i], (gpointer) shell_path);
    }
  }
}
  
gboolean 
verve_env_has_shell (VerveEnv *env, const gchar *name)
{
  return g_hash_table_lookup (env->shells, name) != NULL;
}

/**
 *
 */
const gchar*
verve_env_shell_get_path (VerveEnv *env, const gchar *name)
{
  g_return_val_if_fail (verve_env_has_shell (env, name), name);
  return (const gchar *)g_hash_table_lookup (env->shells, name);
}

GList*
verve_env_get_shell_names (VerveEnv *env)
{
  GList *list = NULL;
  g_hash_table_foreach (env->shells, (GHFunc)verve_env_shell_add_name_to_list, 
                        (gpointer)&list);
  return list;
}

GList*
verve_env_get_shell_paths (VerveEnv *env)
{
  GList *list = NULL;
  g_hash_table_foreach (env->shells, (GHFunc)verve_env_shell_add_path_to_list, 
                        (gpointer)&list);
  return list;
}

void 
verve_env_shell_add_name_to_list (gpointer key, gpointer val, GList **list)
{
  *list = g_list_prepend (*list, key);
}

void 
verve_env_shell_add_path_to_list (gpointer key, gpointer val, GList **list)
{
  *list = g_list_prepend (*list, val);
}

/*********************************************************************
 *
 * PATH parsing and search routines
 *
 *********************************************************************/

gchar **
verve_env_get_path (VerveEnv *env)
{
  static gchar **paths = NULL;
  
  if (G_UNLIKELY (paths == NULL))
    paths = g_strsplit (g_getenv ("PATH"), ":", 0);

  return paths;
}

GList *
verve_env_get_path_binaries (VerveEnv *env)
{
  static GList *binaries = NULL;

  if (G_UNLIKELY (binaries == NULL))
  {
    GError *error = NULL;
    gchar **paths = verve_env_get_path (env);
    int i;
    for (i=0; i<g_strv_length (paths); i++)
    {
      GDir *dir = g_dir_open (paths[i], 0, &error);

      const gchar *current_bin;
      while ((current_bin = g_dir_read_name (dir)) != NULL)
      {
        if (g_find_program_in_path (current_bin))
        {
          binaries = g_list_prepend (binaries, g_strdup (current_bin));
        }
      }
    
      g_dir_close (dir);
    }
  }

  return binaries;
}

/* vim:set expandtab ts=1 sw=2: */
