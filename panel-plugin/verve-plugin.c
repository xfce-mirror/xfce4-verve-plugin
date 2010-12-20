/***************************************************************************
 *            verve-plugin.c
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

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfcegui4/libxfcegui4.h>

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
  GtkWidget        *input;
  
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
  GtkStyle *default_style;
  GtkStyle *style;
  
  g_return_val_if_fail (verve != NULL, FALSE);
  g_return_val_if_fail (verve->input != NULL || GTK_IS_ENTRY (verve->input), FALSE);
  
  /* Determine current entry style */
  style = gtk_widget_get_style (verve->input);

  /* Get default style for widgets */
  default_style = gtk_widget_get_default_style ();
  
  /* Check whether the entry already is highlighted */
  if (gdk_color_equal (&style->base[GTK_STATE_NORMAL], &style->base[GTK_STATE_SELECTED]))
    {
      /* Make it look normal again */
      gtk_widget_modify_base (verve->input, GTK_STATE_NORMAL, &default_style->base[GTK_STATE_NORMAL]);
      gtk_widget_modify_bg (verve->input, GTK_STATE_NORMAL, &default_style->bg[GTK_STATE_NORMAL]);
      gtk_widget_modify_text (verve->input, GTK_STATE_NORMAL, &default_style->text[GTK_STATE_NORMAL]);
    }
  else
    {
      /* Highlight the entry by changing base and background colors */
      gtk_widget_modify_base (verve->input, GTK_STATE_NORMAL, &style->base[GTK_STATE_SELECTED]);
      gtk_widget_modify_bg (verve->input, GTK_STATE_NORMAL, &style->bg[GTK_STATE_SELECTED]);
      gtk_widget_modify_text (verve->input, GTK_STATE_NORMAL, &style->text[GTK_STATE_SELECTED]);
    }
  
  return TRUE;
}



static void
verve_plugin_focus_timeout_reset (VervePlugin *verve)
{
  GtkStyle *default_style;

  g_return_if_fail (verve != NULL);
  g_return_if_fail (verve->input != NULL || GTK_IS_ENTRY (verve->input));

  /* Unregister timeout */
  if (G_LIKELY (verve->focus_timeout != 0))
    {
      g_source_remove (verve->focus_timeout);
      verve->focus_timeout = 0;
    }
  
  /* Get default style */
  default_style = gtk_widget_get_default_style ();
  
  /* Reset entry background */
  gtk_widget_modify_base (verve->input, GTK_STATE_NORMAL, &default_style->base[GTK_STATE_NORMAL]);
  gtk_widget_modify_bg (verve->input, GTK_STATE_NORMAL, &default_style->bg[GTK_STATE_NORMAL]);
  gtk_widget_modify_text (verve->input, GTK_STATE_NORMAL, &default_style->text[GTK_STATE_NORMAL]);
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
  if (event->button != 3 && toplevel && toplevel->window)
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
      xfce_panel_plugin_set_panel_hidden (verve->plugin, FALSE);

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
      case GDK_Escape:
         gtk_entry_set_text (GTK_ENTRY (entry), "");
         return TRUE;

      /* Browse backwards through the command history */
      case GDK_Down:
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
      case GDK_Up:
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
      case GDK_Return:
      case GDK_KP_Enter:
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
        if (G_LIKELY (verve_execute (command, terminal)))
          {
            /* Hide the panel again */
            xfce_panel_plugin_set_panel_hidden (verve->plugin, TRUE);

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
            xfce_err (msg);

            /* Free message */
            g_free (msg);
          }

        /* Free entry text copy */
        g_free (command);

        return TRUE;

      /* Cycle through completion results */
      case GDK_Tab:
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

  /* Init thread system */
  g_thread_init (NULL);

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

  /* Connect to load-binaries signal of environment */
  g_signal_connect (G_OBJECT (verve_env_get()), "load-binaries", G_CALLBACK (verve_plugin_load_completion), verve);

  /* Initialize focus timeout */
  verve->focus_timeout = 0;

  /* Create an event box to put the input entry into */
  verve->event_box = gtk_event_box_new ();
  gtk_widget_show (verve->event_box);
  
  /* Create the input entry */
  verve->input = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (verve->input), 20);
  gtk_widget_show (verve->input);
  gtk_container_add (GTK_CONTAINER (verve->event_box), verve->input);

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



static void
verve_plugin_read_rc_file (XfcePanelPlugin *plugin, 
                           VervePlugin *verve)
{
  XfceRc *rc;
  gchar  *filename;
  
  /* Default size */
  gint    size = 20;

  /* Default number of saved history entries */
  gint    history_length = 25;

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

      /* Read number of saved history entries */
      history_length = xfce_rc_read_int_entry (rc, "history-length", history_length);
    
      /* Update plugin size */
      verve_plugin_update_size (NULL, size, verve);

      /* Update history length */
      verve_plugin_update_history_length (NULL, history_length, verve);
      
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

      /* Write number of saved history entries */
      xfce_rc_write_int_entry (rc, "history-length", verve->history_length);
    
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
verve_plugin_history_length_changed (GtkSpinButton *spin,
                                     VervePlugin   *verve)
{
  g_return_if_fail (verve != NULL);

  /* Update history length */
  verve_plugin_update_history_length (NULL, gtk_spin_button_get_value_as_int (spin), verve);
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
  GtkWidget *frame;
  GtkWidget *bin1;
  GtkWidget *bin2;
  GtkWidget *hbox;
  GtkWidget *size_label;
  GtkWidget *size_spin;
  GtkWidget *history_length_label;
  GtkWidget *history_length_spin;
  GtkObject *adjustment;

  g_return_if_fail (plugin != NULL);
  g_return_if_fail (verve != NULL);

  /* Block plugin context menu */
  xfce_panel_plugin_block_menu (plugin);

  /* Create properties dialog */
  dialog = xfce_titled_dialog_new_with_buttons (_("Verve"),
                                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
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

  /* Frame for appearance settings */
  frame = xfce_create_framebox (_("Appearance"), &bin1);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  /* Plugin size container */
  hbox = gtk_hbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER (bin1), hbox);
  gtk_widget_show (hbox);

  /* Plugin size label */
  size_label = gtk_label_new (_("Width (in chars):"));
  gtk_box_pack_start (GTK_BOX (hbox), size_label, FALSE, TRUE, 0);
  gtk_widget_show (size_label);

  /* Plugin size adjustment */
  adjustment = gtk_adjustment_new (verve->size, 5, 100, 1, 5, 10);

  /* Plugin size spin button */
  size_spin = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
  gtk_widget_add_mnemonic_label (size_spin, size_label);
  gtk_box_pack_start (GTK_BOX (hbox), size_spin, FALSE, TRUE, 0);
  gtk_widget_show (size_spin);

  /* Assign current size to spin button */
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (size_spin), verve->size);

  /* Be notified when the user requests a different plugin size */
  g_signal_connect (size_spin, "value-changed", G_CALLBACK (verve_plugin_size_changed), verve);

  /* Frame for behaviour settings */
  frame = xfce_create_framebox (_("Behaviour"), &bin2);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  /* Plugin history length container */
  hbox = gtk_hbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER (bin2), hbox);
  gtk_widget_show (hbox);

  /* History length label */
  history_length_label = gtk_label_new (_("Number of saved history items:"));
  gtk_box_pack_start (GTK_BOX (hbox), history_length_label, FALSE, TRUE, 0);
  gtk_widget_show (history_length_label);

  /* History length adjustment */
  adjustment = gtk_adjustment_new (verve->history_length, 0, 1000, 1, 5, 10);

  /* History length spin button */
  history_length_spin = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
  gtk_widget_add_mnemonic_label (history_length_spin, history_length_label);
  gtk_box_pack_start (GTK_BOX (hbox), history_length_spin, FALSE, TRUE, 0);
  gtk_widget_show (history_length_spin);

  /* Assign current length to the spin button */
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (history_length_spin), verve->history_length);

  /* Be notified when the user requests a different history length */
  g_signal_connect (history_length_spin, "value-changed", G_CALLBACK (verve_plugin_history_length_changed), verve);

  /* Show properties dialog */
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
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (verve_plugin_construct);



/* vim:set expandtab sts=2 ts=2 sw=2: */
