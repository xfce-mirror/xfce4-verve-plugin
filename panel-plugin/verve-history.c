/***************************************************************************
 *            verve-history.h
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

#include <libxfce4util/libxfce4util.h>

#include "verve.h"
#include "verve-history.h"



const gchar *verve_history_cache_get_filename (void);
static void  verve_history_cache_load         (void);
static void  verve_history_cache_write        (void);
static void  verve_history_append             (gchar *command);



/*********************************************************************
 * 
 * Init / Shutdown functions
 *
 *********************************************************************/

static gint   history_length = 50;
static GList *history = NULL;



void 
verve_history_init (void)
{
  verve_history_cache_load ();
}



void
verve_history_shutdown (void)
{
  /* Write history into the cache file */
  verve_history_cache_write ();

  /* Free history data */
  if (G_LIKELY (history != NULL))
    {
      /* Free entries */
      GList *iter = g_list_first (history);
      while (iter != NULL)
        {
          g_free ((gchar *)iter->data);
          iter = g_list_next (iter);
        }
      
      /* Free list */
      g_list_free (history);
    }
}



void
verve_history_set_length (gint length)
{
  history_length = length;
}



void
verve_history_add (gchar *input)
{
  /* Prepend input to history */
  history = g_list_prepend (history, input);
}



void
verve_history_append (gchar *command)
{
  /* Append command to history */
  history = g_list_append (history, command);
}



GList*
verve_history_begin (void)
{
  /* Return first list entry or NULL */
  return g_list_first (history);
}



GList*
verve_history_end (void)
{
  /* Return last list entry or NULL */
  return g_list_last (history);
}



GList*
verve_history_get_prev (const GList *current)
{
  /* Return command before the current one or NULL */
  return g_list_previous (current);
}



GList*
verve_history_get_next (const GList *current)
{
  /* Return command after the current one or NULL */
  return g_list_next (current);
}



gboolean
verve_history_is_empty (void)
{
  /* Check whether history is uninitialized or its length is zero */
  if (G_UNLIKELY (history == NULL) || g_list_length (history) == 0)
    return TRUE;
  else
    return FALSE;
}



const gchar*
verve_history_get_last_command (void)
{
  /* Get first (= latest) history entry */
  GList *list = verve_history_begin ();
  
  /* Return NULL if list is empty */
  if (G_UNLIKELY (list == NULL))
    return NULL;

  /* Return command data */
  return (gchar *)list->data;
}
    


const gchar *
verve_history_cache_get_filename (void)
{
  static const gchar *filename = "xfce4/Verve/history";
  return filename;
}



static void
verve_history_cache_load (void)
{
  const gchar *basename = verve_history_cache_get_filename ();

  /* Search for cache file */
  gchar *filename = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, basename);

  /* Only read cache if file exists */
  if (G_LIKELY (filename != NULL))
    {
      GError     *error = NULL;
      GIOChannel *handle;
     
      /* Open file input stream */
      handle = g_io_channel_new_file (filename, "r", &error);

      /* Instantly destroy error messages - just ignore them */
      if (G_UNLIKELY (error != NULL))
        g_error_free (error);

      /* Only read contents if stream could be opened */
      if (G_LIKELY (handle != NULL))
        {
          GIOStatus status;
          gchar    *line;
          gsize     length;
          
          /* Read first line */
          status = g_io_channel_read_line (handle, &line, &length, NULL, &error);
          
          /* Read lines until EOF is reached or an error occurs */ 
          while (status != G_IO_STATUS_EOF && error == NULL)
          {
            /* Get current line, remove leading and trailing whitespace */
            GString *strline = g_string_new (g_strstrip (line));

            /* Only add non-empty lines to the history */
            if (G_LIKELY (strline->len > 0))
              verve_history_append (strline->str);

            /* Free string data */
            g_free (line);
            g_string_free (strline, FALSE);
            
            /* Read next line */
            status = g_io_channel_read_line (handle, &line, &length, NULL, &error);
          }

          /* Free error message */
          if (G_UNLIKELY (error != NULL))
            g_error_free (error);

          /* Close file handle */
          g_io_channel_shutdown (handle, TRUE, &error);

          /* Again: Free error message */
          if (G_UNLIKELY (error != NULL))
            g_error_free (error);

          /* Destroy stream object */
          g_io_channel_unref (handle);
        }
    }

  /* Free filename string */
  g_free (filename);
}



static void
verve_history_cache_write (void)
{
  /* Do not write history if it is empty */
  if (verve_history_is_empty ())
    return;

  const gchar *basename = verve_history_cache_get_filename ();

  /* Search for history file, create if it does not exist yet */
  gchar *filename = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, basename, TRUE);

  if (G_LIKELY (filename != NULL))
    {
      GError     *error = NULL;
      GIOChannel *handle;
     
      /* Open output stream */
      handle = g_io_channel_new_file (filename, "w+", &error);

      /* Ignore and free error messages */
      if (G_UNLIKELY (error != NULL))
        g_error_free (error);

      /* Only write contents if stream could be opened */
      if (G_LIKELY (handle != NULL))
      {
        GList    *current;
        gsize     bytes;
        int       i;

        /* Get first history entry */
        current = verve_history_begin();

        /* Save the last 25 commands */
        for (i = 0; i < history_length && current != NULL; i++)
          {
            /* Build output line */
            gchar *line = g_strconcat ("", current->data, "\n", NULL);

            /* Write line */
            g_io_channel_write_chars (handle, line, -1, &bytes, &error);

            /* Free line string */
            g_free (line);
            
            /* Do not write more records if there was an error (e.g. no space left on device) */
            if (G_UNLIKELY (error != NULL))
              {
                g_error_free (error);
                break;
              }
            
            /* Step over to next entry */
            current = verve_history_get_next (current);
          }

        /* Close file handle */
        g_io_channel_shutdown (handle, TRUE, &error);

        /* Free error message */
        if (G_UNLIKELY (error != NULL))
          g_error_free (error);

        /* Destroy stream object */
        g_io_channel_unref (handle);
      }
    }
  
  /* Free filename string */
  g_free (filename);
}

/* vim:set expandtab sts=2 ts=2 sw=2: */
