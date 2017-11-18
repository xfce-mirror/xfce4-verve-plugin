/***************************************************************************
 *            verve-plugin.c
 *
 *  Copyright © 2006-2007 Jannis Pohlmann <jannis@xfce.org>
 *  Copyright © 2015 Isaac Schemm <isaacschemm@gmail.com>
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

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4ui/libxfce4ui.h>

#include "verve.h"
#include "verve-env.h"
#include "verve-history.h"

#ifdef HAVE_DBUS
#include "verve-dbus-service.h"
#endif



typedef struct
{
  XfcePanelPlugin  *plugin;

  /* User interface */
  GtkWidget        *event_box;
  GtkWidget        *label;
  GtkWidget        *input;
  gchar            *fg_color_str;
  gchar            *bg_color_str;
  gchar            *base_color_str;
  
  /* Command history */
  GList            *history_current;

  /* Timeouts */
  guint             focus_timeout;
  
  /* Autocompletion */
  GCompletion      *completion;
  gint              n_complete;

  /* Properties */ 
  gint              size;
  gint              history_length;
  VerveLaunchParams launch_params;

#ifdef HAVE_DBUS
  VerveDBusService *dbus_service;
#endif

} VervePlugin;


/* Mutex lock used to avoid thread collisions when accessing the command
 * completion 
 */
G_LOCK_DEFINE_STATIC (plugin_completion_mutex);


void
verve_plugin_load_completion (VerveEnv* env, gpointer user_data)
{
  VervePlugin *verve = (VervePlugin*) user_data;

  /* Load command history */
  GList *history = verve_history_begin ();

  /* Load linux binaries from PATH */
  GList *binaries = verve_env_get_path_binaries (env);

  /* Merged (and sorted) list */
  GList *items = NULL;

  /* Iterator */
  GList *iter = NULL;

  G_LOCK (plugin_completion_mutex);

  /* Build merged list */
  items = g_list_copy (binaries);
  for (iter = g_list_first (history); iter != NULL; iter = g_list_next (iter))
    items = g_list_insert_sorted (items, iter->data, (GCompareFunc) g_utf8_collate);

  /* Add merged items to completion */
  if (G_LIKELY (history != NULL)) 
    g_completion_add_items (verve->completion, items);

  /* Free merged list */
  g_list_free (items);
  
  G_UNLOCK (plugin_completion_mutex);
}


  
static gboolean
verve_plugin_focus_timeout (VervePlugin *verve)
{
  GtkStyle *style;
  GdkColor c;
  
  g_return_val_if_fail (verve != NULL, FALSE);
  g_return_val_if_fail (verve->input != NULL || GTK_IS_ENTRY (verve->input), FALSE);
  
  /* Determine current entry style */
  style = gtk_widget_get_style (verve->input);
  
  return TRUE;
}



static void
verve_plugin_focus_timeout_reset (VervePlugin *verve)
{
  GtkStyle *style;
  GdkColor c;

  g_return_if_fail (verve != NULL);
  g_return_if_fail (verve->input != NULL || GTK_IS_ENTRY (verve->input));

  /* Unregister timeout */
  if (G_LIKELY (verve->focus_timeout != 0))
    {
      g_source_remove (verve->focus_timeout);
      verve->focus_timeout = 0;
    }
}



static gboolean
verve_plugin_focus_out (GtkWidget *entry,
                        GdkEventFocus *event,
                        VervePlugin *verve)
{
  g_return_val_if_fail (verve != NULL, FALSE);
  g_return_val_if_fail (verve->input != NULL || GTK_IS_ENTRY (verve->input), FALSE);

  /* Stop blinking */
  verve_plugin_focus_timeout_reset (verve);

  return TRUE;
}



static gboolean 
verve_plugin_buttonpress_cb (GtkWidget *entry, 
                             GdkEventButton *event, 
                             VervePlugin *verve)
{
  GtkWidget *toplevel;
  
  g_return_val_if_fail (entry != NULL || GTK_IS_ENTRY (entry), FALSE);
  g_return_val_if_fail (verve != NULL, FALSE);
 
  /* Determine toplevel parent widget */
  toplevel = gtk_widget_get_toplevel (entry);

  /* Reset focus timeout if necessary */
  if (G_LIKELY (verve->focus_timeout != 0))
    verve_plugin_focus_timeout_reset (verve);

  /* Grab entry focus if possible */
  if (event->button != 3 && toplevel && gtk_widget_get_window(toplevel) && !gtk_widget_has_focus(entry))
    xfce_panel_plugin_focus_widget (verve->plugin, entry);

  return FALSE;
}



#ifdef HAVE_DBUS
static void
verve_plugin_grab_focus (VerveDBusService *dbus_service, 
                         VervePlugin *verve)
{
  GtkWidget *toplevel;
 
  g_return_if_fail (verve != NULL);
  g_return_if_fail (verve->input != NULL || GTK_IS_ENTRY (verve->input));
 
  /* Determine toplevel widget */
  toplevel = gtk_widget_get_toplevel (verve->input);

  if (toplevel && toplevel->window)
    {
      /* Unhide the panel */
      xfce_panel_plugin_block_autohide (verve->plugin, TRUE);

      /* Focus the command entry */
      xfce_panel_plugin_focus_widget (verve->plugin, verve->input);
      
      /* Make it flashy so chances are higher that the user notices the focus */
      if (verve->focus_timeout == 0) 
        verve->focus_timeout = g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 250, (GSourceFunc)verve_plugin_focus_timeout, verve, NULL);
    }
}
#endif



static gboolean 
verve_plugin_keypress_cb (GtkWidget   *entry, 
                          GdkEventKey *event, 
                          VervePlugin *verve)
{
  GCompletion *completion;
  gchar       *command;
  gboolean     terminal;
  const gchar *prefix;
  GList       *similar = NULL;
  gboolean     selected = FALSE;
  gint         selstart;
  gint         i;
  gint         len;

  g_return_val_if_fail (verve != NULL, FALSE);

  /* Get entry completion */
  completion = verve->completion;
  
  /* Reset focus timeout, if necessary */
  if (verve->focus_timeout != 0)
    verve_plugin_focus_timeout_reset (verve);
    
  switch (event->keyval)
    {
      /* Reset entry value when ESC is pressed */
      case GDK_KEY_Escape:
         gtk_entry_set_text (GTK_ENTRY (entry), "");
         return TRUE;

      /* Browse backwards through the command history */
      case GDK_KEY_Down:
        /* Do nothing if history is empty */
        if (verve_history_is_empty ())
          return TRUE;

        /* Check if we already are in "history browsing mode" */
        if (G_LIKELY (verve->history_current != NULL))
          {
            /* Get entry before current */
            GList *tmp = verve_history_get_prev (verve->history_current);
            
            /* Make sure we did not reach the end yet */
            if (G_LIKELY (tmp != NULL))
              {
                /* Set current entry */
                verve->history_current = tmp;

                /* Set verve input entry text */
                gtk_entry_set_text (GTK_ENTRY (entry), verve->history_current->data);
              }
            else
              {
                /* Reset pointer to current history entry */
                verve->history_current = NULL;

                /* Clear verve input entry text */
                gtk_entry_set_text (GTK_ENTRY (entry), "");
              }
          }
        else
          {
            /* Get last history entry */
            verve->history_current = verve_history_end();

            /* Set input entry text */
            gtk_entry_set_text (GTK_ENTRY (entry), verve->history_current->data);
          }
        
        return TRUE;

      /* Browse forwards through the command history */
      case GDK_KEY_Up:
        /* Do nothing if the history is empty */
        if (verve_history_is_empty ())
          return TRUE;
        
        /* Check whether we already are in history browsing mode */
        if (G_LIKELY (verve->history_current != NULL))
          {
            /* Get command after current entry */
            GList *tmp = verve_history_get_next (verve->history_current);

            /* Make sure we did not reach the end yet */
            if (G_LIKELY (tmp != NULL))
              {
                /* Set current history entry */
                verve->history_current = tmp;

                /* Set entry text */
                gtk_entry_set_text (GTK_ENTRY (entry), verve->history_current->data);
              }
            else
              {
                /* Reset current history entry */
                verve->history_current = NULL;

                /* Clear entry text */
                gtk_entry_set_text (GTK_ENTRY (entry), "");
              }
          }
        else
          {
            /* Begin with latest history command */
            verve->history_current = verve_history_begin ();

            /* Set entry text */
            gtk_entry_set_text (GTK_ENTRY (entry), verve->history_current->data);
          }
        
        return TRUE;

      /* Execute command entered by the user */
      case GDK_KEY_Return:
      case GDK_KEY_KP_Enter:
        /* Retrieve a copy of the entry text */
        command = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

        /* Remove leading and trailing whitespace */
        command = g_strstrip (command);
        
        /* Run command in terminal if the CTRL key is pressed */
        if ((event->state) & GDK_CONTROL_MASK)
          terminal = TRUE;
        else
          terminal = FALSE;
        
        /* Try executing the command */
        if (G_LIKELY (verve_execute (command, terminal, verve->launch_params)))
          {
            /* Hide the panel again */
            xfce_panel_plugin_block_autohide (verve->plugin, FALSE);

            /* Do not add command to history if it is the same as the one before */
            if (verve_history_is_empty () || g_utf8_collate (verve_history_get_last_command (), command) != 0)
              {
                /* Add command to history */
                verve_history_add (g_strdup (command));

                G_LOCK (plugin_completion_mutex);

                /* Add command to completion */
                verve->completion->items = g_list_insert_sorted (verve->completion->items, g_strdup (command), (GCompareFunc) g_utf8_collate);

                G_UNLOCK (plugin_completion_mutex);
              }
      
            /* Reset current history entry */
            verve->history_current = NULL;

            /* Clear input entry text */
            gtk_entry_set_text (GTK_ENTRY (entry), "");
          }
        else
          {
            /* Generate error message */
            gchar *msg = g_strconcat (_("Could not execute command:"), " ", command, NULL);

            /* Display error message dialog */
            xfce_dialog_show_error (NULL, NULL, "%s", msg);

            /* Free message */
            g_free (msg);
          }

        /* Free entry text copy */
        g_free (command);

        return TRUE;

      /* Cycle through completion results */
      case GDK_KEY_Tab:
        /* Retrieve a copy of the entry text */
        command = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

        /* Determine command length and abort if it is empty */
        if ((len = g_utf8_strlen (command, -1)) == 0)
          return TRUE;

        /* Determine currently selected chars */
        selected = gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &selstart, NULL);
       
        /* Check if we are in auto-completion browsing mode */
        if (selected && selstart != 0)
          {
            /* Switch over to the next completion result */
            verve->n_complete++;

            /* Determine the prefix (which is what the user actually typed by himself) */
            prefix = g_strndup (command, selstart);
          }
        else
          {
            /* Start with first completion result */
            verve->n_complete = 0;

            /* The complete input is the prefix */
            prefix = command;
          }

        G_LOCK (plugin_completion_mutex);

        /* Get all completion results */
        similar = g_completion_complete (completion, prefix, NULL);

        G_UNLOCK (plugin_completion_mutex);

        /* Check if there are any results */
        if (G_LIKELY (similar != NULL))
          {
            /* Check if we are in browsing mode already */
            if (selected && selstart != 0)
              {
                /* Go back to the first entry if we reached the end of the result list */
                if (verve->n_complete >= g_list_length (similar))
                  verve->n_complete = 0;

                /* Search the next result */
                for (i=0; i<verve->n_complete; i++)
                  if (similar->next != NULL)
                    similar = similar->next;
              }

            /* Put result text into input entry */
            gtk_entry_set_text (GTK_ENTRY (entry), similar->data);

            /* Select chars after the prefix entered by the user */
            gtk_editable_select_region (GTK_EDITABLE (entry), (selstart == 0 ? len : selstart), -1);
          }

        /* Free entry text copy */
        g_free (command);
 
        return TRUE;

      /* Any other key pressed, so the entry will handle the input itself */
      default:
        return FALSE;
    }
}



static VervePlugin*
verve_plugin_new (XfcePanelPlugin *plugin)
{
  /* Set application name */
  g_set_application_name ("Verve");

#if !GLIB_CHECK_VERSION (2, 30, 0)
  /* Init thread system */
  g_thread_init (NULL);
#endif

  /* Init Verve mini-framework */
  verve_init ();
  
  /* Create the plugin object */
  VervePlugin *verve = g_new (VervePlugin, 1);

  /* Assign the panel plugin to the plugin member */
  verve->plugin = plugin;
  
  /* Initialize completion variables */
  verve->history_current = NULL;
  verve->completion = g_completion_new (NULL);
  verve->n_complete = 0;
  verve->size = 20;
  verve->history_length = 25;
  verve->launch_params.use_bang = FALSE;
  verve->launch_params.use_backslash = FALSE;
  verve->launch_params.use_smartbookmark = FALSE;
  verve->launch_params.smartbookmark_url = g_strdup ("");
  
  /* Initialize colors */
  verve->fg_color_str = g_strdup ("");
  verve->bg_color_str = g_strdup ("");
  verve->base_color_str = g_strdup ("");

  /* Initialize label */
  verve->label = gtk_label_new ("");

  /* Connect to load-binaries signal of environment */
  g_signal_connect (G_OBJECT (verve_env_get()), "load-binaries", G_CALLBACK (verve_plugin_load_completion), verve);

  /* Initialize focus timeout */
  verve->focus_timeout = 0;

  /* Create an event box to put the input entry into */
  verve->event_box = gtk_event_box_new ();
  gtk_widget_show (verve->event_box);
  
  /* Create a container for the label and input */
  GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (verve->event_box), hbox);
  gtk_widget_show (hbox);

  /* Add the label */
  gtk_widget_show (verve->label);
  gtk_container_add (GTK_CONTAINER (hbox), verve->label);
  
  /* Create the input entry */
  verve->input = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (verve->input), 20);
  gtk_widget_show (verve->input);
  gtk_container_add (GTK_CONTAINER (hbox), verve->input);

  /* Handle mouse button and key press events */
  g_signal_connect (verve->input, "key-press-event", G_CALLBACK (verve_plugin_keypress_cb), verve);
  g_signal_connect (verve->input, "button-press-event", G_CALLBACK (verve_plugin_buttonpress_cb), verve);
  g_signal_connect (verve->input, "focus-out-event", G_CALLBACK (verve_plugin_focus_out), verve);

#ifdef HAVE_DBUS
  /* Attach the D-BUS service */
  verve->dbus_service = g_object_new (VERVE_TYPE_DBUS_SERVICE, NULL);

  /* Connect to D-BUS service signals */
  g_signal_connect (G_OBJECT (verve->dbus_service), "open-dialog", G_CALLBACK (verve_plugin_grab_focus), verve);
  g_signal_connect (G_OBJECT (verve->dbus_service), "grab-focus", G_CALLBACK (verve_plugin_grab_focus), verve);
#endif
  
  return verve;
}



static void
verve_plugin_free (XfcePanelPlugin *plugin, 
                   VervePlugin *verve)
{
#ifdef HAVE_DBUS
  g_object_unref (G_OBJECT (verve->dbus_service));
#endif

  /* Unregister focus timeout */
  verve_plugin_focus_timeout_reset (verve);

  /* Unload completion */
  g_completion_free (verve->completion);

  /* Free plugin data structure */
  g_free (verve);

  /* Shutdown Verve mini-framework */
  verve_shutdown ();
}



static gboolean
verve_plugin_update_size (XfcePanelPlugin *plugin, gint size, VervePlugin *verve)
{
  g_return_val_if_fail (verve != NULL, FALSE);
  g_return_val_if_fail (verve->input != NULL || GTK_IS_ENTRY (verve->input), FALSE);

  /* Set internal size variable */
  verve->size = size;

  /* Update entry width */
  gtk_entry_set_width_chars (GTK_ENTRY (verve->input), size);

  return TRUE;
}



static gboolean
verve_plugin_size_changed_request (XfcePanelPlugin *plugin, 
                                   gint size, 
                                   VervePlugin *verve)
{
  g_return_val_if_fail (verve != NULL, FALSE);

  /* Update Verve size */
  verve_plugin_update_size (plugin, verve->size, verve);

  return TRUE;
}



static gboolean
verve_plugin_update_label (XfcePanelPlugin *plugin,
                           const gchar     *label,
                           VervePlugin     *verve)
{
  g_return_val_if_fail (verve != NULL, FALSE);

  /* Set text in internal label object */
  gtk_label_set_text(verve->label, label);

  return TRUE;
}



static gboolean
verve_plugin_update_colors (XfcePanelPlugin *plugin,
                            const gchar     *fg_color_str,
                            const gchar     *bg_color_str,
                            const gchar     *base_color_str,
                            VervePlugin     *verve)
{
  g_return_val_if_fail (verve != NULL, FALSE);

  GdkColor c;
  if (fg_color_str) {
    if (verve->fg_color_str) {
      g_free (verve->fg_color_str);
    }
    verve->fg_color_str = g_strdup (fg_color_str);
  }
  if (bg_color_str) {
    if (verve->bg_color_str) {
      g_free (verve->bg_color_str);
    }
    verve->bg_color_str = g_strdup (bg_color_str);
  }
  if (base_color_str) {
    if (verve->base_color_str) {
      g_free (verve->base_color_str);
    }
    verve->base_color_str = g_strdup (base_color_str);
  }

  return TRUE;
}



static gboolean
verve_plugin_update_history_length (XfcePanelPlugin *plugin,
                                    gint             history_length,
                                    VervePlugin     *verve)
{
  g_return_val_if_fail (verve != NULL, FALSE);

  /* Set internal history length variable */
  verve->history_length = history_length;

  /* Apply length to history */
  verve_history_set_length (history_length);

  return TRUE;
}



static gboolean
verve_plugin_update_smartbookmark_url (XfcePanelPlugin *plugin,
                                       const gchar     *url,
                                       VervePlugin     *verve)
{
  g_return_val_if_fail (verve != NULL, FALSE);

  /* Set internal search engine URL variable */
  g_free (verve->launch_params.smartbookmark_url);
  verve->launch_params.smartbookmark_url = g_strdup(url);

  return TRUE;
}



static void
verve_plugin_read_rc_file (XfcePanelPlugin *plugin, 
                           VervePlugin *verve)
{
  XfceRc *rc;
  gchar  *filename;
  
  /* Default size */
  gint    size = 20;

  /* Default label */
  gchar  *label = "";
  
  /* Default foreground and background colors */
  gchar  *fg_color_str = "";
  gchar  *bg_color_str = "";
  gchar  *base_color_str = "";

  /* Default number of saved history entries */
  gint    history_length = 25;

  /* Default launch parameters */
  verve->launch_params.use_url = TRUE;
  verve->launch_params.use_email = TRUE;
  verve->launch_params.use_dir = TRUE;
  verve->launch_params.use_wordexp = TRUE;
  verve->launch_params.use_bang = FALSE;
  verve->launch_params.use_backslash = FALSE;
  verve->launch_params.use_smartbookmark = FALSE;
  verve->launch_params.use_shell = TRUE;

  /* Default search engine URL */
  gchar *smartbookmark_url = "";

  g_return_if_fail (plugin != NULL);
  g_return_if_fail (verve != NULL);

  /* Search for config file */
  filename = xfce_panel_plugin_lookup_rc_file (plugin);

  /* Abort if file could not be found */
  if (G_UNLIKELY (filename == NULL))
    return;

  /* Open rc handle */
  rc = xfce_rc_simple_open (filename, TRUE);

  /* Only read config if rc handle could be opened */
  if (G_LIKELY (rc != NULL))
    {
      /* Read size value */
      size = xfce_rc_read_int_entry (rc, "size", size);

      /* Read label text */
      label = xfce_rc_read_entry (rc, "label", label);

      /* Read foreground and background colors */
      fg_color_str = xfce_rc_read_entry (rc, "foreground-color", fg_color_str);
      bg_color_str = xfce_rc_read_entry (rc, "background-color", bg_color_str);
      base_color_str = xfce_rc_read_entry (rc, "base-color", base_color_str);

      /* Read number of saved history entries */
      history_length = xfce_rc_read_int_entry (rc, "history-length", history_length);

      /* Read launch parameters */
      verve->launch_params.use_url = xfce_rc_read_bool_entry (rc, "use-url", verve->launch_params.use_url);
      verve->launch_params.use_email = xfce_rc_read_bool_entry (rc, "use-email", verve->launch_params.use_email);
      verve->launch_params.use_dir = xfce_rc_read_bool_entry (rc, "use-dir", verve->launch_params.use_dir);
      verve->launch_params.use_wordexp = xfce_rc_read_bool_entry (rc, "use-wordexp", verve->launch_params.use_wordexp);
      verve->launch_params.use_bang = xfce_rc_read_bool_entry (rc, "use-bang", verve->launch_params.use_bang);
      verve->launch_params.use_backslash = xfce_rc_read_bool_entry (rc, "use-backslash", verve->launch_params.use_backslash);
      verve->launch_params.use_smartbookmark = xfce_rc_read_bool_entry (rc, "use-smartbookmark", verve->launch_params.use_smartbookmark);
      verve->launch_params.use_shell = xfce_rc_read_bool_entry (rc, "use-shell", verve->launch_params.use_shell);

      /* Read smartbookmark URL */
      smartbookmark_url = xfce_rc_read_entry (rc, "smartbookmark-url", smartbookmark_url);
    
      /* Update plugin size */
      verve_plugin_update_size (NULL, size, verve);
    
      /* Update label */
      verve_plugin_update_label (NULL, label, verve);
    
      /* Update colors */
      verve_plugin_update_colors (NULL, fg_color_str, bg_color_str, base_color_str, verve);

      /* Update history length */
      verve_plugin_update_history_length (NULL, history_length, verve);

      /* Update smartbookmark URL */
      verve_plugin_update_smartbookmark_url (NULL, smartbookmark_url, verve);
      
      /* Close handle */
      xfce_rc_close (rc);
    }

  /* Free filename string */
  g_free (filename);
}



static void
verve_plugin_write_rc_file (XfcePanelPlugin *plugin, 
                            VervePlugin *verve)
{
  XfceRc *rc;
  gchar *filename;

  g_return_if_fail (plugin != NULL);
  g_return_if_fail (verve != NULL);

  /* Search for the config file */
  filename = xfce_panel_plugin_save_location (plugin, TRUE);

  /* Abort saving if the file does not exists */
  if (G_UNLIKELY (filename == NULL))
    return;

  /* Open rc handle */
  rc = xfce_rc_simple_open (filename, FALSE);

  if (G_LIKELY (rc != NULL))
    {
      /* Write size value */
      xfce_rc_write_int_entry (rc, "size", verve->size);

      /* Write label value */
      xfce_rc_write_entry (rc, "label", gtk_label_get_text(verve->label));

      /* Write number of saved history entries */
      xfce_rc_write_int_entry (rc, "history-length", verve->history_length);

      /* Write launch param settings */
      xfce_rc_write_bool_entry (rc, "use-url", verve->launch_params.use_url);
      xfce_rc_write_bool_entry (rc, "use-email", verve->launch_params.use_email);
      xfce_rc_write_bool_entry (rc, "use-dir", verve->launch_params.use_dir);
      xfce_rc_write_bool_entry (rc, "use-wordexp", verve->launch_params.use_wordexp);
      xfce_rc_write_bool_entry (rc, "use-bang", verve->launch_params.use_bang);
      xfce_rc_write_bool_entry (rc, "use-backslash", verve->launch_params.use_backslash);
      xfce_rc_write_bool_entry (rc, "use-smartbookmark", verve->launch_params.use_smartbookmark);
      xfce_rc_write_bool_entry (rc, "use-shell", verve->launch_params.use_shell);

      /* Write smartbookmark URL */
      xfce_rc_write_entry (rc, "smartbookmark-url", verve->launch_params.smartbookmark_url);

      /* Write colors */
      xfce_rc_write_entry (rc, "foreground-color", verve->fg_color_str ? verve->fg_color_str : "");
      xfce_rc_write_entry (rc, "background-color", verve->bg_color_str ? verve->bg_color_str : "");
      xfce_rc_write_entry (rc, "base-color", verve->base_color_str ? verve->base_color_str : "");
    
      /* Close handle */
      xfce_rc_close (rc);
    }

  /* Free filename string */
  g_free (filename);
}



static void
verve_plugin_size_changed (GtkSpinButton *spin, 
                           VervePlugin *verve)
{
  g_return_if_fail (verve != NULL);

  /* Update plugin size */
  verve_plugin_update_size (NULL, gtk_spin_button_get_value_as_int (spin), verve);
}



static void
verve_plugin_fg_color_changed (GtkEntry *box, 
                               VervePlugin *verve)
{
  g_return_if_fail (verve != NULL);

  /* Get the entered color */
  const gchar *color_str = gtk_entry_get_text (box);

  verve_plugin_update_colors (NULL, color_str, NULL, NULL, verve);
}



static void
verve_plugin_base_color_changed (GtkEntry *box, 
                                 VervePlugin *verve)
{
  g_return_if_fail (verve != NULL);

  /* Get the entered color */
  const gchar *color_str = gtk_entry_get_text (box);

  verve_plugin_update_colors (NULL, NULL, NULL, color_str, verve);
}



static void
verve_plugin_label_changed (GtkEntry *box, 
                            VervePlugin *verve)
{
  g_return_if_fail (verve != NULL);

  /* Get the entered URL */
  const gchar *label = gtk_entry_get_text (box);

  /* Update search engine ID */
  verve_plugin_update_label (NULL, label, verve);
}



static void
verve_plugin_history_length_changed (GtkSpinButton *spin,
                                     VervePlugin   *verve)
{
  g_return_if_fail (verve != NULL);

  /* Update history length */
  verve_plugin_update_history_length (NULL, gtk_spin_button_get_value_as_int (spin), verve);
}



static void
verve_plugin_use_url_changed (GtkToggleButton *button, 
                              VervePlugin     *verve)
{
  g_return_if_fail (verve != NULL);
  verve->launch_params.use_url = gtk_toggle_button_get_active (button);
}



static void
verve_plugin_use_email_changed (GtkToggleButton *button, 
                                VervePlugin     *verve)
{
  g_return_if_fail (verve != NULL);
  verve->launch_params.use_email = gtk_toggle_button_get_active (button);
}



static void
verve_plugin_use_dir_changed (GtkToggleButton *button, 
                              VervePlugin     *verve)
{
  g_return_if_fail (verve != NULL);
  verve->launch_params.use_dir = gtk_toggle_button_get_active (button);
}



static void
verve_plugin_use_wordexp_changed (GtkToggleButton *button, 
                                  VervePlugin     *verve)
{
  g_return_if_fail (verve != NULL);
  verve->launch_params.use_wordexp = gtk_toggle_button_get_active (button);
}



static void
verve_plugin_use_bang_changed (GtkToggleButton *button, 
                               VervePlugin     *verve)
{
  g_return_if_fail (verve != NULL);
  verve->launch_params.use_bang = gtk_toggle_button_get_active (button);
}



static void
verve_plugin_use_backslash_changed (GtkToggleButton *button, 
                                    VervePlugin     *verve)
{
  g_return_if_fail (verve != NULL);
  verve->launch_params.use_backslash = gtk_toggle_button_get_active (button);
}



static void
verve_plugin_use_smartbookmark_changed (GtkToggleButton *button, 
                                        VervePlugin     *verve)
{
  g_return_if_fail (verve != NULL);
  verve->launch_params.use_smartbookmark = gtk_toggle_button_get_active (button);
}



static void
verve_plugin_use_shell_changed (GtkToggleButton *button, 
                                VervePlugin     *verve)
{
  g_return_if_fail (verve != NULL);
  verve->launch_params.use_shell = gtk_toggle_button_get_active (button);
}



static void
verve_plugin_smartbookmark_url_changed (GtkEntry    *box, 
                                        VervePlugin *verve)
{
  g_return_if_fail (verve != NULL);

  /* Get the entered URL */
  const gchar *smartbookmark_url = gtk_entry_get_text(box);

  /* Update search engine ID */
  verve_plugin_update_smartbookmark_url (NULL, smartbookmark_url, verve);
}



static void
verve_plugin_response (GtkWidget *dialog, 
                       int response, 
                       VervePlugin *verve)
{
  g_return_if_fail (verve != NULL);
  g_return_if_fail (verve->plugin != NULL);

  /* Disconnect from dialog */
  g_object_set_data (G_OBJECT (verve->plugin), "dialog", NULL);

  /* Destroy dialog object */
  gtk_widget_destroy (dialog);
  
  /* Unblock plugin context menu */
  xfce_panel_plugin_unblock_menu (verve->plugin);

  /* Save changes to config file */
  verve_plugin_write_rc_file (verve->plugin, verve);
}



static void
verve_plugin_properties (XfcePanelPlugin *plugin, 
                         VervePlugin *verve)
{
  GtkWidget *dialog;
  GtkWidget *notebook;
  GtkWidget *notebook_tab_label_current;
  
  GtkWidget *general_vbox;
  GtkWidget *frame;
  GtkWidget *bin1;
  GtkWidget *bin2;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *size_label;
  GtkWidget *size_spin;
  GtkWidget *base_color_label;
  GtkWidget *base_color_box;
  GtkWidget *fg_color_label;
  GtkWidget *fg_color_box;
  GtkWidget *label_label;
  GtkWidget *label_box;
  GtkWidget *history_length_label;
  GtkWidget *history_length_spin;
  GObject *adjustment;

  GtkWidget *bin3;
  GtkWidget *alignment;
  GtkWidget *command_types_vbox;
  GtkWidget *command_types_label1;
  GtkWidget *command_types_label2;
  GtkWidget *command_type_url;
  GtkWidget *command_type_email;
  GtkWidget *command_type_dir;
  GtkWidget *command_type_use_wordexp;
  GtkWidget *command_type_bang;
  GtkWidget *command_type_backslash;
  GtkWidget *command_type_smartbookmark;
  GtkWidget *engine_box;
  GtkWidget *command_type_executable;
  GtkWidget *command_type_use_shell;

  gchar     *color_str;

  g_return_if_fail (plugin != NULL);
  g_return_if_fail (verve != NULL);

  /* Block plugin context menu */
  xfce_panel_plugin_block_menu (plugin);

  /* Create properties dialog */
  dialog = xfce_titled_dialog_new_with_buttons (_("Verve"),
                                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                                                NULL);

  /* Set dialog property */
  g_object_set_data (G_OBJECT (plugin), "dialog", dialog);

  /* Be notified when the properties dialog is closed */
  g_signal_connect (dialog, "response", G_CALLBACK (verve_plugin_response), verve);

  /* Basic dialog window setup */
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "utilities-terminal");
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 2);
  
  /* Notebook setup */
  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), notebook, TRUE, TRUE, 0);
  gtk_widget_show (notebook);
  
  general_vbox = gtk_vbox_new (FALSE, 8);
  notebook_tab_label_current = gtk_label_new (_("General"));
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), general_vbox, notebook_tab_label_current);
  gtk_widget_show(general_vbox);

  /* Frame for appearance settings */
  frame = xfce_gtk_frame_box_new (_("Appearance"), &bin1);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_box_pack_start (GTK_BOX (general_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /* Appearance settings vertical container */
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER(bin1), vbox);
  gtk_widget_show(vbox);

  /* Plugin size container */
  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* Plugin size label */
  size_label = gtk_label_new (_("Width (in chars):"));
  gtk_box_pack_start (GTK_BOX (hbox), size_label, FALSE, TRUE, 0);
  gtk_widget_show (size_label);

  /* Plugin size adjustment */
  adjustment = gtk_adjustment_new (verve->size, 5, 300, 1, 5, 0);

  /* Plugin size spin button */
  size_spin = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
  gtk_widget_add_mnemonic_label (size_spin, size_label);
  gtk_box_pack_start (GTK_BOX (hbox), size_spin, FALSE, TRUE, 0);
  gtk_widget_show (size_spin);

  /* Assign current size to spin button */
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (size_spin), verve->size);

  /* Be notified when the user requests a different plugin size */
  g_signal_connect (size_spin, "value-changed", G_CALLBACK (verve_plugin_size_changed), verve);

  /* Plugin label container */
  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* Plugin label label */
  label_label = gtk_label_new (_("Label:"));
  gtk_box_pack_start (GTK_BOX (hbox), label_label, FALSE, TRUE, 0);
  gtk_widget_show (label_label);

  /* Plugin label entry field */
  label_box = gtk_entry_new();

  /* Set text to current plugin label */
  gtk_entry_set_text(GTK_ENTRY (label_box), gtk_label_get_text(GTK_LABEL (verve->label)));
  gtk_widget_add_mnemonic_label (label_box, label_label);
  gtk_box_pack_start (GTK_BOX (hbox), label_box, FALSE, TRUE, 0);
  gtk_widget_show (label_box);

  /* Be notified when the user requests a different label setting */
  g_signal_connect (label_box, "changed", G_CALLBACK (verve_plugin_label_changed), verve);

  /* Frame for color settings */
  frame = xfce_gtk_frame_box_new (_("Colors"), &bin1);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_box_pack_start (GTK_BOX (general_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /* Color settings vertical container */
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER(bin1), vbox);
  gtk_widget_show(vbox);

  /* Plugin background color container */
  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* Plugin background color label */
  base_color_label = gtk_label_new (_("Background color:"));
  gtk_box_pack_start (GTK_BOX (hbox), base_color_label, FALSE, TRUE, 0);
  gtk_widget_show (base_color_label);

  /* Plugin background color entry field */
  base_color_box = gtk_entry_new();

  /* Set text to current background color */
  if (verve->base_color_str) {
    gtk_entry_set_text(GTK_ENTRY (base_color_box), verve->base_color_str);
  }
  gtk_widget_add_mnemonic_label (base_color_box, base_color_label);
  gtk_box_pack_start (GTK_BOX (hbox), base_color_box, FALSE, TRUE, 0);
  gtk_widget_show (base_color_box);

  /* Be notified when the user requests a different background color setting */
  g_signal_connect (base_color_box, "changed", G_CALLBACK (verve_plugin_base_color_changed), verve);

  /* Plugin foreground color container */
  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* Plugin foreground color label */
  fg_color_label = gtk_label_new (_("Foreground color:"));
  gtk_box_pack_start (GTK_BOX (hbox), fg_color_label, FALSE, TRUE, 0);
  gtk_widget_show (fg_color_label);

  /* Plugin foreground color entry field */
  fg_color_box = gtk_entry_new();

  /* Set text to current foreground color */
  if (verve->fg_color_str) {
    gtk_entry_set_text(GTK_ENTRY (fg_color_box), verve->fg_color_str);
  }
  gtk_widget_add_mnemonic_label (fg_color_box, fg_color_label);
  gtk_box_pack_start (GTK_BOX (hbox), fg_color_box, FALSE, TRUE, 0);
  gtk_widget_show (fg_color_box);

  /* Be notified when the user requests a different foreground color setting */
  g_signal_connect (fg_color_box, "changed", G_CALLBACK (verve_plugin_fg_color_changed), verve);

  /* Frame for behaviour settings */
  frame = xfce_gtk_frame_box_new (_("History"), &bin2);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_box_pack_start (GTK_BOX (general_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);
  
  /* Behaviour settings vertical container */
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER(bin2), vbox);
  gtk_widget_show(vbox);

  /* Plugin history length container */
  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* History length label */
  history_length_label = gtk_label_new (_("Number of saved history items:"));
  gtk_box_pack_start (GTK_BOX (hbox), history_length_label, FALSE, TRUE, 0);
  gtk_widget_show (history_length_label);

  /* History length adjustment */
  adjustment = gtk_adjustment_new (verve->history_length, 0, 1000, 1, 5, 0);

  /* History length spin button */
  history_length_spin = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
  gtk_widget_add_mnemonic_label (history_length_spin, history_length_label);
  gtk_box_pack_start (GTK_BOX (hbox), history_length_spin, FALSE, TRUE, 0);
  gtk_widget_show (history_length_spin);

  /* Assign current length to the spin button */
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (history_length_spin), verve->history_length);

  /* Be notified when the user requests a different history length */
  g_signal_connect (history_length_spin, "value-changed", G_CALLBACK (verve_plugin_history_length_changed), verve);
  
  /* Second tab */
  frame = xfce_gtk_frame_box_new (_("Behaviour"), &bin3);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_widget_show (frame);
  
  notebook_tab_label_current = gtk_label_new (_("Behaviour"));
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, notebook_tab_label_current);
  gtk_widget_show (frame);
  
  command_types_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (bin3), command_types_vbox);
  gtk_widget_show (command_types_vbox);

  /* Pattern types frame label */
  command_types_label1 = gtk_label_new(_("Enable support for:"));
  gtk_misc_set_alignment (GTK_MISC (command_types_label1), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (command_types_vbox), command_types_label1, FALSE, TRUE, 8);
  gtk_widget_show (command_types_label1);
  
  /* Command type: URL */
  command_type_url = gtk_check_button_new_with_label(_("URLs (http/https/ftp/ftps)"));
  gtk_box_pack_start (GTK_BOX (command_types_vbox), command_type_url, FALSE, TRUE, 0);
  gtk_widget_show (command_type_url);
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (command_type_url), verve->launch_params.use_url);
  g_signal_connect (command_type_url, "toggled", G_CALLBACK (verve_plugin_use_url_changed), verve);
  
  /* Command type: email address */
  command_type_email = gtk_check_button_new_with_label(_("Email addresses"));
  gtk_box_pack_start (GTK_BOX (command_types_vbox), command_type_email, FALSE, TRUE, 0);
  gtk_widget_show (command_type_email);
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (command_type_email), verve->launch_params.use_email);
  g_signal_connect (command_type_email, "toggled", G_CALLBACK (verve_plugin_use_email_changed), verve);
  
  /* Command type: directory path */
  command_type_dir = gtk_check_button_new_with_label(_("Directory paths"));
  gtk_box_pack_start (GTK_BOX (command_types_vbox), command_type_dir, FALSE, TRUE, 0);
  gtk_widget_show (command_type_dir);
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (command_type_dir), verve->launch_params.use_dir);
  g_signal_connect (command_type_dir, "toggled", G_CALLBACK (verve_plugin_use_dir_changed), verve);

  /* wordexp checkbox */
  alignment = gtk_alignment_new (1, 1, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 24, 0);
  gtk_box_pack_start (GTK_BOX (command_types_vbox), alignment, FALSE, TRUE, 0);
  gtk_widget_show (alignment);
  
  command_type_use_wordexp = gtk_check_button_new_with_label(_("Expand variables with wordexp"));
  gtk_container_add (GTK_CONTAINER (alignment), command_type_use_wordexp);
#ifdef HAVE_WORDEXP
  gtk_widget_show (command_type_use_wordexp);
#endif
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (command_type_use_wordexp), verve->launch_params.use_wordexp);
  g_signal_connect (command_type_use_wordexp, "toggled", G_CALLBACK (verve_plugin_use_wordexp_changed), verve);
  
  /* Command type: !bang */
  command_type_bang = gtk_check_button_new_with_label(_("DuckDuckGo queries (starting with !)"));
  gtk_box_pack_start (GTK_BOX (command_types_vbox), command_type_bang, FALSE, TRUE, 0);
  gtk_widget_show (command_type_bang);
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (command_type_bang), verve->launch_params.use_bang);
  g_signal_connect (command_type_bang, "toggled", G_CALLBACK (verve_plugin_use_bang_changed), verve);
  
  /* Command type: I'm feeling ducky (\) */
  command_type_backslash = gtk_check_button_new_with_label(_("DuckDuckGo queries (starting with \\)"));
  gtk_box_pack_start (GTK_BOX (command_types_vbox), command_type_backslash, FALSE, TRUE, 0);
  gtk_widget_show (command_type_backslash);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (command_type_backslash), verve->launch_params.use_backslash);
  g_signal_connect (command_type_backslash, "toggled", G_CALLBACK (verve_plugin_use_backslash_changed), verve);
  
  /* Fallback if the above don't match */
  command_types_label2 = gtk_label_new(_("If the above patterns don't match:"));
  gtk_misc_set_alignment (GTK_MISC (command_types_label2), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (command_types_vbox), command_types_label2, FALSE, TRUE, 8);
  gtk_widget_show (command_types_label2);
  
  /* Smart bookmark radio button */
  command_type_smartbookmark = gtk_radio_button_new_with_label(NULL, _("Use smart bookmark URL"));
  gtk_box_pack_start (GTK_BOX (command_types_vbox), command_type_smartbookmark, FALSE, TRUE, 0);
  gtk_widget_show (command_type_smartbookmark);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (command_type_smartbookmark), verve->launch_params.use_smartbookmark);
  g_signal_connect (command_type_smartbookmark, "toggled", G_CALLBACK (verve_plugin_use_smartbookmark_changed), verve);

  /* Smart bookmark entry box */
  engine_box = gtk_entry_new();
  alignment = gtk_alignment_new (1, 1, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 8, 8, 24, 0);
  gtk_box_pack_start (GTK_BOX (command_types_vbox), alignment, FALSE, TRUE, 0);
  gtk_widget_show (alignment);
  gtk_container_add (GTK_CONTAINER (alignment), engine_box);
  gtk_widget_show (engine_box);

  /* Set text to search engine URL */
  gtk_entry_set_text(GTK_ENTRY (engine_box), verve->launch_params.smartbookmark_url);

  /* Be notified when the user requests a different smartbookmark url setting */
  g_signal_connect (engine_box, "changed", G_CALLBACK (verve_plugin_smartbookmark_url_changed), verve);
  
  /* Executable command radio button (smart bookmark off) */
  command_type_executable = gtk_radio_button_new_with_label(gtk_radio_button_get_group (GTK_RADIO_BUTTON (command_type_smartbookmark)), _("Run as executable command"));
  gtk_box_pack_start (GTK_BOX (command_types_vbox), command_type_executable, FALSE, TRUE, 0);
  gtk_widget_show (command_type_executable);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (command_type_executable), !verve->launch_params.use_smartbookmark);

  /* Use shell checkbox */
  alignment = gtk_alignment_new (1, 1, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 24, 0);
  gtk_box_pack_start (GTK_BOX (command_types_vbox), alignment, FALSE, TRUE, 0);
  gtk_widget_show (alignment);
  
  command_type_use_shell = gtk_check_button_new_with_label(_("Run command with $SHELL -i -c\n(enables alias and variable expansion)"));
  gtk_container_add (GTK_CONTAINER (alignment), command_type_use_shell);
  gtk_widget_show (command_type_use_shell);
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (command_type_use_shell), verve->launch_params.use_shell);
  g_signal_connect (command_type_use_shell, "toggled", G_CALLBACK (verve_plugin_use_shell_changed), verve);

  /* Show properties dialog */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
  gtk_widget_show (dialog);
}
 


static void
verve_plugin_construct (XfcePanelPlugin *plugin)
{
  /* Set gettext text domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
  
  /* Create Verve plugin */
  VervePlugin *verve = verve_plugin_new (plugin);

  /* Read config file */
  verve_plugin_read_rc_file (plugin, verve);
 
  /* Add event box to the panel plugin */
  gtk_container_add (GTK_CONTAINER (plugin), verve->event_box);
  xfce_panel_plugin_add_action_widget (plugin, verve->event_box);

  /* Make the plugin configurable from the context menu */
  xfce_panel_plugin_menu_show_configure (plugin);
 
  /* Connect to panel plugin signals */
  g_signal_connect (plugin, "save", G_CALLBACK (verve_plugin_write_rc_file), verve);
  g_signal_connect (plugin, "free-data", G_CALLBACK (verve_plugin_free), verve);
  g_signal_connect (plugin, "configure-plugin", G_CALLBACK (verve_plugin_properties), verve);
  g_signal_connect (plugin, "size-changed", G_CALLBACK (verve_plugin_size_changed_request), verve);
}
 


/* Register exteral panel plugin */
XFCE_PANEL_PLUGIN_REGISTER (verve_plugin_construct);



/* vim:set expandtab sts=2 ts=2 sw=2: */
