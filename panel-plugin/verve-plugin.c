/***************************************************************************
 *            verve-plugin.c
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

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfcegui4/libxfcegui4.h>
#include "../verve/verve.h"
#include "../verve/verve-env.h"
#include "../verve/verve-db.h"
#include "../verve/verve-history.h"

typedef struct
{
  XfcePanelPlugin *plugin;

  /* User interface */
  GtkWidget *event_box;
  GtkWidget *input;
  
  /* Command database */
  VerveDb *commands;

  /* Command history */
  GList *history_current;
  
  /* Autocompletion */
  GCompletion *completion;
  gint n_complete;

  /* Properties */
  gint size;

} VervePlugin;

GCompletion *
verve_plugin_load_completion (VerveDb *db)
{
	 /* Load commands */
	 GList *commands = verve_db_get_command_names (db);
	 GCompletion *completion = g_completion_new (NULL);

	 if (G_LIKELY (commands != NULL))
		  g_completion_add_items (completion, commands);

	 /* Load linux binaries from PATH */
	 VerveEnv *env = verve_env_get ();
	 g_completion_add_items (completion, verve_env_get_path_binaries (env));
	
	 return completion;
}

static gboolean verve_plugin_buttonpress_cb (GtkWidget *entry, GdkEventButton *event, gpointer data)
{
	 static Atom atom = 0;
	 GtkWidget *toplevel = gtk_widget_get_toplevel (entry);

	 if (event->button != 3 && toplevel && toplevel->window)
	 {
		  XClientMessageEvent xev;

		  if (G_UNLIKELY (!atom))
			   atom = XInternAtom (GDK_DISPLAY (), "_NET_ACTIVE_WINDOW", FALSE);

		  xev.type = ClientMessage;
	  	xev.window = GDK_WINDOW_XID (toplevel->window);
		  xev.message_type = atom;
		  xev.format = 32;
		  xev.data.l[0] = 0;
  		xev.data.l[1] = 0;
    xev.data.l[2] = 0;
		  xev.data.l[3] = 0;
		  xev.data.l[4] = 0;

		  XSendEvent (GDK_DISPLAY (), GDK_ROOT_WINDOW (), False,
				            StructureNotifyMask, (XEvent *)&xev);

		  gtk_widget_grab_focus (entry);
	 }

	 return FALSE;
}

static gboolean verve_plugin_keypress_cb (GtkWidget *entry, GdkEventKey *event, gpointer user_data)
{
  VervePlugin *verve = (VervePlugin *)user_data;
  GCompletion *completion = verve->completion;

  gchar *command;
	 gboolean selected = FALSE;
	 const gchar *prefix;
	 GList *similar = NULL;
	 gint selstart;
	 gint i;
	 gint len;
		
	 switch (event->keyval)
	 {
		  case GDK_Escape:
			   gtk_entry_set_text (GTK_ENTRY (entry), "");
			   return TRUE;

    case GDK_Down:
      if (verve_history_is_empty ())
        return TRUE;

      if (!verve->history_current)
      {
        verve->history_current = verve_history_end();
        gtk_entry_set_text (GTK_ENTRY (entry), verve->history_current->data);
        return TRUE;
      }
      else
      {
        GList *tmp = verve_history_get_prev (verve->history_current);
        if (!tmp)
        {
          verve->history_current = NULL;
          gtk_entry_set_text (GTK_ENTRY (entry), "");
        }
        else
        {
          verve->history_current = tmp;
          gtk_entry_set_text (GTK_ENTRY (entry), verve->history_current->data);
        }
      }
      
      return TRUE;

    case GDK_Up:
      if (verve_history_is_empty ())
        return TRUE;
        
      if (!verve->history_current)
      {
        verve->history_current = verve_history_begin ();
        gtk_entry_set_text (GTK_ENTRY (entry), verve->history_current->data);
      }
      else
      {
        GList *tmp = verve_history_get_next (verve->history_current);
        if (!tmp)
        {
          verve->history_current = NULL;
          gtk_entry_set_text (GTK_ENTRY (entry), "");
        }
        else
        {
          verve->history_current = tmp;
          gtk_entry_set_text (GTK_ENTRY (entry), verve->history_current->data);
        }
      }
      
      return TRUE;

		  case GDK_Return:
      command = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
      command = g_strstrip (command);
      if (!verve_execute (command))
      {
        show_error (_("Unkown command."));
			   }
      else
      {
        verve_history_add (g_strdup (command));
        verve->history_current = NULL;
        gtk_entry_set_text (GTK_ENTRY (entry), "");
      }
      g_free (command);
			   return TRUE;

    case GDK_Tab:
		  	 command = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
			
			   if ((len = g_utf8_strlen (command, -1)) == 0)
			    	return TRUE;

			   selected = gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &selstart, NULL);
			
			   if (selected && selstart != 0)
			   {
				    verve->n_complete++;
				    prefix = g_strndup (command, selstart);
			   }
			   else
			   {
				    verve->n_complete = 0;
				    prefix = command;
			   }

			   similar = g_completion_complete (completion, prefix, NULL);

			   if (G_LIKELY (similar != NULL))
			   {
				    if (selected && selstart != 0)
				    {
					     if (verve->n_complete >= g_list_length (similar))
						      verve->n_complete = 0;

    				  for (i=0; i<verve->n_complete; i++)
						      if (similar->next != NULL)
							       similar = similar->next;
				    }

				    if (!verve_db_has_command (verve->commands, similar->data))
				    {
          gchar *path = g_find_program_in_path (similar->data);
					     if (G_LIKELY (path == NULL))
					     {
						      return TRUE;
					     }
					     g_free (path);
				    }
    
				    gtk_entry_set_text (GTK_ENTRY (entry), similar->data);
				    gtk_editable_select_region (GTK_EDITABLE (entry), (selstart == 0 ? len : selstart), -1);
			   }
      g_free (command);
			   return TRUE;

		  default:
			   return FALSE;
	 }
}

static VervePlugin *
verve_plugin_new (XfcePanelPlugin *plugin)
{
 	g_set_application_name ("Verve");

 	verve_init ();
	
 	VervePlugin *verve = g_new (VervePlugin, 1);
 	verve->plugin = plugin;
  
 	verve->commands = verve_db_get ();
  verve->history_current = verve_history_begin();
 	verve->completion = verve_plugin_load_completion (verve->commands);
 	verve->n_complete = 0;
  verve->size = 20;

 	verve->event_box = gtk_event_box_new ();
 	gtk_widget_show (verve->event_box);
	
 	verve->input = gtk_entry_new ();
 	gtk_entry_set_width_chars (GTK_ENTRY (verve->input), 20);
 	gtk_widget_show (verve->input);
 	gtk_container_add (GTK_CONTAINER (verve->event_box), verve->input);

 	g_signal_connect (verve->input, "key-press-event", 
	 		G_CALLBACK (verve_plugin_keypress_cb), verve);
 	g_signal_connect (verve->input, "button-press-event", 
 			G_CALLBACK (verve_plugin_buttonpress_cb), NULL);
	
 	return verve;
}

static void
verve_plugin_free (XfcePanelPlugin *plugin, VervePlugin *verve)
{
 	g_free (verve->completion);
 	g_free (verve);
 	verve_shutdown ();
}

static void
verve_plugin_update_size (VervePlugin *verve, gint size)
{
  verve->size = size;
  gtk_entry_set_width_chars (GTK_ENTRY (verve->input), size);
}

static void
verve_plugin_read_rc_file (XfcePanelPlugin *plugin, VervePlugin *verve)
{
  gchar *filename;
  XfceRc *rc;
  
  /* Default size */
  gint size = 20;

  if (!(filename = xfce_panel_plugin_lookup_rc_file (plugin)))
    return;

  rc = xfce_rc_simple_open (filename, TRUE);
  g_free (filename);

  if (!rc)
    return;

  size = xfce_rc_read_int_entry (rc, "size", size);

  xfce_rc_close (rc);

  verve_plugin_update_size (verve, size);
}

static void
verve_plugin_write_rc_file (XfcePanelPlugin *plugin, VervePlugin *verve)
{
  gchar *filename;
  XfceRc *rc;

  if (!(filename = xfce_panel_plugin_save_location (plugin, TRUE)))
    return;

  rc = xfce_rc_simple_open (filename, FALSE);
  g_free (filename);

  if (!rc)
    return;

  xfce_rc_write_int_entry (rc, "size", verve->size);

  xfce_rc_close (rc);
}

static void
verve_plugin_size_changed (GtkSpinButton *spin, VervePlugin *verve)
{
  verve_plugin_update_size (verve, gtk_spin_button_get_value_as_int (spin));
}

static void
verve_plugin_response (GtkWidget *dialog, int response, VervePlugin *verve)
{
  g_object_set_data (G_OBJECT (verve->plugin), "dialog", NULL);

  gtk_widget_destroy (dialog);
  
  xfce_panel_plugin_unblock_menu (verve->plugin);
  verve_plugin_write_rc_file (verve->plugin, verve);
}

static void
verve_plugin_properties (XfcePanelPlugin *plugin, VervePlugin *verve)
{
  GtkWidget *dialog, *header, *frame, *bin, *hbox, *size_label, *size_spin;
  GtkObject *adjustment;

  xfce_panel_plugin_block_menu (plugin);

  dialog = gtk_dialog_new_with_buttons (_("Verve Properties"),
                 GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                 GTK_DIALOG_DESTROY_WITH_PARENT |
                 GTK_DIALOG_NO_SEPARATOR,
                 GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                 NULL);
  
  g_object_set_data (G_OBJECT (plugin), "dialog", dialog);

  g_signal_connect (dialog, "response", G_CALLBACK (verve_plugin_response), verve);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 2);

  header = xfce_create_header (NULL, _("Verve"));
  gtk_widget_set_size_request (GTK_BIN (header)->child, 200, 32);
  gtk_container_set_border_width (GTK_CONTAINER (header), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), header, FALSE, TRUE, 0);

  frame = xfce_create_framebox (_("Appearance"), &bin);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_widget_show (frame);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (bin), hbox);

  adjustment = gtk_adjustment_new (verve->size, 5, 100, 1, 5, 10);

  size_label = gtk_label_new (_("Width (in chars):"));
  gtk_widget_show (size_label);
  gtk_box_pack_start (GTK_BOX (hbox), size_label, FALSE, FALSE, 0);

  size_spin = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
  gtk_widget_add_mnemonic_label (size_spin, size_label);
  gtk_widget_show (size_spin);
  gtk_box_pack_start (GTK_BOX (hbox), size_spin, TRUE, TRUE, 0);

  g_signal_connect (size_spin, "value-changed", G_CALLBACK (verve_plugin_size_changed), verve);

  gtk_widget_show (dialog);
}

static void
verve_plugin_construct (XfcePanelPlugin *plugin)
{
 	xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
 	
 	VervePlugin *verve = verve_plugin_new (plugin);

  verve_plugin_read_rc_file (plugin, verve);
 
 	gtk_container_add (GTK_CONTAINER (plugin), verve->event_box);
  
 	xfce_panel_plugin_add_action_widget (plugin, verve->event_box);
  xfce_panel_plugin_menu_show_configure (plugin);
 
 	g_signal_connect (plugin, "save", G_CALLBACK (verve_plugin_write_rc_file), verve);
 	g_signal_connect (plugin, "free-data", G_CALLBACK (verve_plugin_free), verve);
  g_signal_connect (plugin, "configure-plugin", G_CALLBACK (verve_plugin_properties), verve);
}
 
XFCE_PANEL_PLUGIN_REGISTER_INTERNAL (verve_plugin_construct);

/* vim:set expandtab ts=1 sw=2: */
