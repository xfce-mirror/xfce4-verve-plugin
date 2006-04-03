/***************************************************************************
 *            verve-env.c
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

#include "verve-env.h"



static void verve_env_class_init             (VerveEnvClass *klass);
static void verve_env_init                   (VerveEnv      *env);
static void verve_env_finalize               (GObject       *object);



struct _VerveEnvClass
{
  GObjectClass __parent__;
};



struct _VerveEnv
{
  GObject __parent__;

  /* $PATH list */
  gchar **paths;

  /* Binaries in $PATH */
  GList  *binaries;
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
  
  /* Determine parent class */
  verve_env_parent_class = g_type_class_peek_parent (klass);
  
  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = verve_env_finalize;
}



void 
verve_env_init (VerveEnv *env)
{
  env->paths = NULL;
  env->binaries = NULL;
}



static VerveEnv *env = NULL;  



VerveEnv*
verve_env_get (void)
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
verve_env_shutdown (void)
{
  if (G_LIKELY (env != NULL))
    g_object_unref (G_OBJECT (env));
}



static void 
verve_env_finalize (GObject *object)
{
  VerveEnv *env = VERVE_ENV (object);

  /* Free path list */
  if (G_LIKELY (env->paths != NULL))
    g_strfreev (env->paths);

  /* Free binaries list */
  if (G_LIKELY (env->binaries != NULL))
    {
      GList *iter = g_list_first (env->binaries);
      while (iter != NULL)
        {
          g_free ((gchar *)iter->data);
          iter = g_list_next (iter);
        }
      g_list_free (env->binaries);
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
    env->paths = g_strsplit (g_getenv ("PATH"), ":", 0);

  return env->paths;
}



GList *
verve_env_get_path_binaries (VerveEnv *env)
{
  if (G_UNLIKELY (env->binaries == NULL))
    {
      gchar **paths;
      int     i;
      
      /* Get $PATH directories */
      paths = verve_env_get_path (env);
      
      /* Iterate over paths list */
      for (i=0; i<g_strv_length (paths); i++)
      {
        GError *error = NULL;

        /* Try opening the directory */
        GDir *dir = g_dir_open (paths[i], 0, &error);

        /* Skip directory when errors have occured */
        if (G_LIKELY (error == NULL))
          {
            const gchar *current;

            /* Iterate over files in this directory */
            while ((current = g_dir_read_name (dir)) != NULL)
              {
                /* Add file filename to the list */
                env->binaries = g_list_prepend (env->binaries, g_strdup (current));
              }

            /* Close directory */
            g_dir_close (dir);
          }
        else
          g_error_free (error);
      }
    }

  return env->binaries;
}



/* vim:set expandtab sts=2 ts=2 sw=2: */
