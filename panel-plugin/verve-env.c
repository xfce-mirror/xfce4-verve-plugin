/***************************************************************************
 *            verve-env.c
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

#include "verve-env.h"



static void     verve_env_class_init             (gpointer       g_class,
                                                  gpointer       class_data);
static void     verve_env_init                   (GTypeInstance *instance,
                                                  gpointer       g_class);
static void     verve_env_finalize               (GObject       *object);
static void     verve_env_load_binaries          (VerveEnv      *env);
static gpointer verve_env_load_thread            (gpointer       user_data);



struct _VerveEnvClass
{
  GObjectClass __parent__;

  guint        load_binaries_signal;

  /* Signals */
  void (*load_binaries) (VerveEnv *env);
};



struct _VerveEnv
{
  GObject  __parent__;

  /* $PATH list */
  gchar  **paths;

  /* Binaries in $PATH */
  GList   *binaries;

  /* Thread used for loading $PATH binary names */
  gboolean load_thread_cancelled;
  GThread *load_thread;
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
        verve_env_class_init,
        NULL,
        NULL,
        sizeof (VerveEnv),
        0,
        verve_env_init,
        NULL,
      };
      
      type = g_type_register_static (G_TYPE_OBJECT, "VerveEnv", &info, 0);
    }
    
  return type;
}



void
verve_env_class_init (gpointer g_class,
                      gpointer class_data)
{
  GObjectClass  *gobject_class = g_class;
  VerveEnvClass *klass = g_class;

  /* Determine parent class */
  verve_env_parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = verve_env_finalize;

  klass->load_binaries = verve_env_load_binaries;

  /* Register "load-binaries" signal */
  klass->load_binaries_signal = g_signal_new ("load-binaries", 
                                              G_TYPE_FROM_CLASS (klass), 
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET (VerveEnvClass, load_binaries),
                                              NULL, NULL,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE,
                                              0);
}



void 
verve_env_init (GTypeInstance *instance,
                gpointer       g_class)
{
  VerveEnv *env = VERVE_ENV (instance);

  env->paths = NULL;
  env->binaries = NULL;

  /* Spawn the thread used to load the command completion data */
  env->load_thread = g_thread_new (NULL, verve_env_load_thread, env);
}



static VerveEnv *global_env = NULL;



VerveEnv*
verve_env_get (void)
{
  if (G_UNLIKELY (global_env == NULL))
    {
      global_env = g_object_new (VERVE_TYPE_ENV, NULL);
      g_object_add_weak_pointer (G_OBJECT (global_env), (gpointer) &global_env);
    }
  else
    g_object_ref (G_OBJECT (global_env));
  
  return global_env;
}



void 
verve_env_shutdown (void)
{
  if (G_LIKELY (global_env != NULL))
    g_object_unref (G_OBJECT (global_env));
}



static void 
verve_env_finalize (GObject *object)
{
  VerveEnv *env = VERVE_ENV (object);

  /* Cancel and join the loading thread */
  env->load_thread_cancelled = TRUE;
  g_thread_join (env->load_thread);

  /* Free path list */
  if (G_LIKELY (env->paths != NULL))
    g_strfreev (env->paths);

  /* Free binaries list */
  if (G_LIKELY (env->binaries != NULL))
    {
      g_list_free_full (env->binaries, g_free);
      env->binaries = NULL;
    }
}



/*********************************************************************
 *
 * PATH parsing and search routines
 *
 *********************************************************************/

gchar**
verve_env_get_path (VerveEnv *env)
{
  if (G_UNLIKELY (env->paths == NULL))
    env->paths = g_strsplit (g_getenv ("PATH"), G_SEARCHPATH_SEPARATOR_S, -1);

  return env->paths;
}



GList *
verve_env_get_path_binaries (VerveEnv *env)
{
  return env->binaries;
}



static gpointer
verve_env_load_thread (gpointer user_data)
{
  VerveEnv *env = VERVE_ENV (user_data);
  gchar   **paths;
  guint       i;
  
  /* Get $PATH directories */
  paths = verve_env_get_path (env);
  
  /* Iterate over paths list */
  for (i = 0; !env->load_thread_cancelled && i < g_strv_length (paths); i++)
  {
    const gchar *current;
    gchar       *filename;
    GList       *lp;
    /* Try opening the directory */
    GDir *dir = g_dir_open (paths[i], 0, NULL);

    /* Continue with next directory if this one cant' be opened */
    if (G_UNLIKELY (dir == NULL)) 
      continue;

    /* Iterate over files in this directory */
    while (!env->load_thread_cancelled && (current = g_dir_read_name (dir)) != NULL)
      {
        /* Convert to valid UTF-8 */
        filename = g_filename_display_name (current);

        /* Avoid duplicates */
        for (lp = g_list_first (env->binaries); lp != NULL; lp = lp->next)
          if (g_ascii_strcasecmp (lp->data, filename) == 0)
            break;
       
        /* Check details of file if it's not in the list already */
        if (G_LIKELY (lp == NULL))
          {
            /* Determine the absolute path to the file */
            gchar *path = g_build_filename (paths[i], current, NULL);

            /* Check if the path refers to an executable */
            if (g_file_test (path, G_FILE_TEST_IS_EXECUTABLE) &&
                !g_file_test (path, G_FILE_TEST_IS_DIR))
              {
                /* Add file filename to the list */
                env->binaries = g_list_prepend (env->binaries, filename);

                /* No need to free the filename later in this function */
                filename = NULL;
              }

            /* Free absolute path */
            g_free (path);
          }

        /* Release filename if necessary */
        g_free (filename);
      }

    /* Close directory */
    g_dir_close (dir);
  }

  /* Sort binaries */
  env->binaries = g_list_sort (env->binaries, (GCompareFunc) g_utf8_collate);

  /* Emit 'load-binaries' signal */
  g_signal_emit_by_name (env, "load-binaries");

  return env->binaries;
}



static void
verve_env_load_binaries (VerveEnv *env)
{
  /* Do nothing */
}



/* vim:set expandtab sts=2 ts=2 sw=2: */
