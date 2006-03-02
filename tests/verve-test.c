/***************************************************************************
 *            test.c
 *
 *  $Id: test.c 1017 2006-02-07 17:57:22Z jpohlmann $
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

#include <glib.h>
#include <glib-object.h>
#include "../verve/verve.h"

int 
main (int argc, char **argv)
{
  /* Init GObject type system */
  g_type_init ();

  /* Init GLib thread */
  if (!g_thread_supported ())
    g_thread_init (NULL);

  g_set_application_name ("VerveTest");

  /* Init Verve backend */
  verve_init ();

  /* Init command database */
  VerveDb *commands = verve_db_get ();

  /* Init environment */
  VerveEnv *env = verve_env_get ();

  /* Init command history */
  GList *history = verve_history_begin ();
 
  /* Execute a command (URL)
  gchar *cmd = g_strdup ("   http://xfce.org    ");
  cmd = g_strstrip (cmd);
  verve_execute (cmd);
  verve_history_add (g_strdup (cmd));
  g_free (cmd);
  */

  /* Execute another command
  cmd = g_strdup ("pdf");
  cmd = g_strstrip (cmd);
  verve_execute (cmd);
  verve_history_add (g_strdup (cmd));
  g_free (cmd);
  */
  
  /* Shutdown Verve backend */
  verve_shutdown ();
  
  return 0;
}

/* vim:set expandtab ts=1 sw=2: */
