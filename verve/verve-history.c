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

#include <libxfce4util/libxfce4util.h>
#include "verve.h"
#include "verve-history.h"

const gchar *_verve_history_cache_get_filename (void);
static void _verve_history_cache_load (void);
static void _verve_history_cache_write (void);
static void _verve_history_append (gchar *command);

/*********************************************************************
 * 
 * Init / Shutdown functions
 *
 *********************************************************************/

static GList *history = NULL;

void 
_verve_history_init (void)
{
  _verve_history_cache_load ();
}

void
_verve_history_shutdown (void)
{
  _verve_history_cache_write ();

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

void
_verve_history_append (gchar *command)
{
  history = g_list_append (history, command);
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
    
const gchar *
_verve_history_cache_get_filename (void)
{
  static const gchar *filename = "xfce4/Verve/history";
  return filename;
}

static void
_verve_history_cache_load (void)
{
  const gchar *basename = _verve_history_cache_get_filename ();
  gchar *filename = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, basename);

  if (G_UNLIKELY (filename == NULL))
    return;

  GError *error = NULL;
  GIOChannel *handle = g_io_channel_new_file (filename, "r", &error);

  if (error)
    g_error_free (error);

  if (G_LIKELY (handle != NULL))
  {
    gchar *line;
    gsize length;
    GIOStatus status;
    
    status = g_io_channel_read_line (handle, &line, &length, NULL, &error);
    while (status != G_IO_STATUS_EOF && error == NULL)
    {
      GString *strline = g_string_new (g_strstrip (line));
      g_free (line);

      if (strline->len > 0)
      {
        _verve_history_append (strline->str);
      } 

      g_string_free (strline, FALSE);
      
      status = g_io_channel_read_line (handle, &line, &length, NULL, &error);
    }

    if (error)
      g_error_free (error);

    g_io_channel_shutdown (handle, TRUE, &error);

    if (error)
      g_error_free (error);

    g_io_channel_unref (handle);
  }

  g_free (filename);
}

static void
_verve_history_cache_write (void)
{
  if (verve_history_is_empty ())
    return;

  const gchar *basename = _verve_history_cache_get_filename ();
  gchar *filename = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, basename, TRUE);

  if (G_UNLIKELY (filename == NULL))
    return;

  GError *error = NULL;
  GIOChannel *handle = g_io_channel_new_file (filename, "w+", &error);

  if (error)
    g_error_free (error);

  if (G_LIKELY (handle != NULL))
  {
    GList *current = verve_history_begin();
    
    GIOStatus status;
    gsize bytes;
    
    int i;
    for (i=0; i<25, current != NULL; i++) /* Cache the last 25 commands */
    {
      g_io_channel_write_chars (handle, g_strconcat ("", current->data, "\n", NULL), -1, &bytes, &error);
      
      if (error)
        break;
      
      current = verve_history_get_next (current);
    }

    g_io_channel_shutdown (handle, TRUE, &error);

    if (error)
      g_error_free (error);

    g_io_channel_unref (handle);
  }
  
  g_free (filename);
}

/* vim:set expandtab ts=1 sw=2: */
