/***************************************************************************
 *            verve-env.h
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
 
#ifndef __VERVE_ENV_H__
#define __VERVE_ENV_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _VerveEnvClass VerveEnvClass;
typedef struct _VerveEnv VerveEnv;
  
#define VERVE_TYPE_ENV            (verve_env_get_type ())
#define VERVE_ENV(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VERVE_TYPE_ENV, VerveEnv))
#define VERVE_ENV_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass, VERVE_TYPE_ENV, VerveEnvClass)))
#define VERVE_IS_ENV(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VERVE_TYPE_ENV))
#define VERVE_IS_ENV_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VERVE_TYPE_ENV))
#define VERVE_ENV_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VERVE_TYPE_ENV, VerveEnvClass))

GType verve_env_get_type (void) G_GNUC_CONST;
VerveEnv *verve_env_get (void);
gboolean verve_env_has_shell (VerveEnv *env, const gchar *name);
const gchar *verve_env_shell_get_path (VerveEnv *env, const gchar *name);
const gchar *verve_env_get_shell (void);
GList *verve_env_get_shell_names (VerveEnv *env);
GList *verve_env_get_shell_paths (VerveEnv *env);
gchar **verve_env_get_path (VerveEnv *env);
GList *verve_env_get_path_binaries (VerveEnv *env);

void _verve_env_shutdown (void);

G_END_DECLS;

#endif /* __VERVE_ENV_H__ */

/* vim:set expandtab ts=1 sw=2: */
