/***************************************************************************
 *            verve-history.h
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

#include "verve.h"
#include "verve-history.h"

/*********************************************************************
 * 
 * Init / Shutdown functions
 *
 *********************************************************************/

static GList *history = NULL;

void 
_verve_history_init (void)
{
}

void
_verve_history_shutdown (void)
{
  if (G_LIKELY (history != NULL))
  {
    GList *iter = history;
    while (iter != NULL)
    {
      g_free ((gchar *)iter->data);
      iter = g_list_next (iter);
    }
    g_list_free (history);
  }
}

void
verve_history_add (gchar *input)
{
  history = g_list_prepend (history, input);
}

GList *
verve_history_begin (void)
{
  return g_list_first (history);
}

GList *
verve_history_end (void)
{
  return g_list_last (history);
}

GList *
verve_history_get_prev (const GList *current)
{
  return g_list_previous (current);
}

GList *
verve_history_get_next (const GList *current)
{
  return g_list_next (current);
}

gboolean
verve_history_is_empty (void)
{
  if (G_UNLIKELY (history == NULL) || g_list_length (history) == 0)
    return TRUE;
  else
    return FALSE;
}
    

/* vim:set expandtab ts=1 sw=2: */
