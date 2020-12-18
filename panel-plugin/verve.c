/***************************************************************************
 *            verve.c
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

#include "config.h"

#include <pcre.h>

#include <glib-object.h>

#ifdef HAVE_WORDEXP
#include <wordexp.h>
#endif

#include <libxfce4ui/libxfce4ui.h>

#include "verve.h"
#include "verve-env.h"
#include "verve-history.h"



static gboolean verve_is_url       (const gchar *str);
static gboolean verve_is_email     (const gchar *str);
static gchar *verve_is_directory   (const gchar *str, gboolean use_wordexp);



/* URL/eMail matching patterns */
#define USERCHARS   "-A-Za-z0-9"
#define PASSCHARS   "-A-Za-z0-9,?;.:/!%$^*&~\"#'"
#define HOSTCHARS   "-A-Za-z0-9"
#define USER        "[" USERCHARS "]+(:["PASSCHARS "]+)?"
#define MATCH_URL1  "^((file|https?|ftps?)://(" USER "@)?)[" HOSTCHARS ".]+(:[0-9]+)?" \
                    "(/[-A-Za-z0-9_$.+!*(),;:@&=?/~#%]*[^]'.}>) \t\r\n,\\\"])?/?$"
#define MATCH_URL2  "^(www|ftp)[" HOSTCHARS "]*\\.[" HOSTCHARS ".]+(:[0-9]+)?" \
                    "(/[-A-Za-z0-9_$.+!*(),;:@&=?/~#%]*[^]'.}>) \t\r\n,\\\"])?/?$"
#define MATCH_EMAIL "^(mailto:)?[a-z0-9][a-z0-9.-]*@[a-z0-9][a-z0-9-]*(\\.[a-z0-9][a-z0-9-]*)+$"

/*********************************************************************
 *
 * Initialize/shutdown Verve
 * -------------------------
 *
 * The execution of these functions is necessary before and after 
 * your program makes use of Verve.
 *
 *********************************************************************/

void 
verve_init (void)
{
  /* Init history database */
  verve_history_init ();
}



void
verve_shutdown (void)
{
  /* Free history database */
  verve_history_shutdown ();

  /* Shutdown environment */
  verve_env_shutdown ();
}



static void
verve_command_callback (GPid pid,
                        gint status,
                        gpointer data)
{
  status >>= 8;
  if (status == 126 || status == 127)
  {
    xfce_dialog_show_error (NULL, NULL, _("Could not execute command (exit status %d)"), status);
  }
  g_spawn_close_pid (pid);
}



static void verve_setsid (gpointer p)
{
    setsid();
}

/*********************************************************************
 *
 * Verve command line execution function
 * -------------------------------------
 *
 * With the help of this function, shell commands can be executed.
 *
 *********************************************************************/
 
gboolean verve_spawn_command_line (const gchar *cmdline)
{
  gint         argc;
  gchar      **argv;
  gboolean     success;
  GPid         child_pid;
  GError      *error = NULL;
  const gchar *home_dir;
  GSpawnFlags  flags;
  
  /* Parse command line arguments */
  success = g_shell_parse_argv (cmdline, &argc, &argv, &error);

  /* Return false if command line arguments failed to be arsed */
  if (G_UNLIKELY (error != NULL))
    {
      g_error_free (error);
      return FALSE;
    }

  /* Get user's home directory */
  home_dir = xfce_get_homedir ();

  /* Set up spawn flags */
  flags = G_SPAWN_STDOUT_TO_DEV_NULL;
  flags |= G_SPAWN_STDERR_TO_DEV_NULL;
  flags |= G_SPAWN_SEARCH_PATH;
  flags |= G_SPAWN_DO_NOT_REAP_CHILD;
  
  /* Spawn subprocess */
  success = g_spawn_async (home_dir, argv, NULL, flags, verve_setsid, NULL, &child_pid, &error);

  /* Return false if subprocess could not be spawned */
  if (G_UNLIKELY (error != NULL)) 
    {
      g_error_free (error);
      return FALSE;
    }

  /* Free command line arguments */
  g_strfreev (argv);

  g_child_watch_add(child_pid, verve_command_callback, NULL);

  /* Return whether process was spawned successfully */
  return success;
}



/*********************************************************************
 * 
 * Verve main execution method
 * ---------------------------
 *
 * This method should be used whenever you want to run command line 
 * input, be it a URL, an eMail address or a custom command.
 *
 *********************************************************************/

gboolean
verve_execute (const gchar *input, 
               gboolean terminal,
               VerveLaunchParams launch_params)
{
  gchar   *command;
  gchar   *directory_exp;
  gboolean result = FALSE;
    
  /* Open URLs, eMail addresses and directories using exo-open */
  if ((launch_params.use_email && verve_is_email (input)) || (launch_params.use_url && verve_is_url (input)))
  {
    /* Build exo-open command */
    command = g_strconcat ("exo-open ", input, NULL);
  }
  else if (launch_params.use_dir && (directory_exp = verve_is_directory (input, launch_params.use_wordexp)))
  {
    /* Build exo-open command */
    command = g_strconcat ("exo-open ", directory_exp, NULL);
    g_free (directory_exp);
  }
  else if ((launch_params.use_bang && input[0] == '!') || (launch_params.use_backslash && input[0] == '\\'))
  {
    /* Launch DuckDuckGo */
    gchar *esc_input = g_uri_escape_string(input, NULL, TRUE);
    command = g_strconcat ("exo-open https://duckduckgo.com/?q=", esc_input, NULL);
    g_free(esc_input);
  }
  else if (launch_params.use_smartbookmark)
  {
    /* Launch user-defined search engine */
    gchar *esc_input = g_uri_escape_string(input, NULL, TRUE);
    command = g_strconcat ("exo-open ", launch_params.smartbookmark_url, esc_input, NULL);
    g_free(esc_input);
  }
  else
  {
    if (launch_params.use_shell)
    {
      /* Find current shell */
      char *shell = getenv("SHELL");
      if (shell == NULL) shell = "/bin/sh";
      
      gchar *quoted_input = g_shell_quote (input);
      command = g_strconcat (shell, " -i -c ", quoted_input, NULL);
      g_free (quoted_input);
    }
    else
    {
      command = g_strdup (input);
    }
    
    /* Run command using the xfterm4 wrapper if the terminal flag was set */
    if (G_UNLIKELY (terminal))
    {
      gchar *quoted_command = g_shell_quote (command);
      
      g_free (command);
      command = g_strconcat ("exo-open --launch TerminalEmulator ", quoted_command, NULL);
      
      g_free (quoted_command);
    }
  }
    
  /* Try to execute the exo-open command */
  if (verve_spawn_command_line (command))
    result = TRUE;
    
  /* Free command string */
  g_free (command);

  /* Return spawn result */
  return result;
}



/*********************************************************************
 *
 * Internal pattern matching functions
 *
 *********************************************************************/

gboolean
verve_is_url (const gchar *str)
{
  GString     *string = g_string_new (str);
  pcre        *pattern;
  const gchar *error;
  int          error_offset;
  int          ovector[30];
  gboolean     success = FALSE;

  /* Compile first pattern */
  pattern = pcre_compile (MATCH_URL1, 0, &error, &error_offset, NULL);
  
  /* Test whether the string matches this pattern */
  if (pcre_exec (pattern, NULL, string->str, string->len, 0, 0, ovector, 30) >= 0)
    success = TRUE;

  /* Free pattern */
  pcre_free (pattern);

  /* Free data and return true if string matched the first pattern */
  if (success)
    {
      g_string_free (string, TRUE);
      return TRUE;
    }

  /* Compile second pattern */
  pattern = pcre_compile (MATCH_URL2, 0, &error, &error_offset, NULL);

  /* Test whether the string matches this pattern */
  if (pcre_exec (pattern, NULL, string->str, string->len, 0, 0, ovector, 30) >= 0)
    success = TRUE;

  /* Free pattern */
  pcre_free (pattern);

  /* Free string */
  g_string_free (string, TRUE);

  /* Return whether the string matched any of the URL patterns */
  return success;
}



gboolean
verve_is_email (const gchar *str)
{
  GString     *string = g_string_new (str);
  const gchar *error;
  pcre        *pattern;
  int          error_offset;
  int          ovector[30];
  gboolean     success = FALSE;

  /* Compile the pattern */
  pattern = pcre_compile (MATCH_EMAIL, 0, &error, &error_offset, NULL);

  /* Test whether the string matches this pattern */
  if (pcre_exec (pattern, NULL, string->str, string->len, 0, 0, ovector, 30) >= 0)
    success = TRUE;

  /* Free pattern */
  pcre_free (pattern);

  /* Free string data */
  g_string_free (string, TRUE);

  /* Return whether the string matched any of the eMail patterns */
  return success;
}



static gchar *
verve_is_directory (const gchar *str,
                    gboolean use_wordexp)
{
#ifdef HAVE_WORDEXP
  if (use_wordexp) {
    wordexp_t w;
    int result;

    /* Avoid opening directories with the same name as an existing executable. */
    if (g_find_program_in_path (str))
      return NULL;

    /* Run wordexp with command substitution turned off */
    result = wordexp(str, &w, WRDE_NOCMD);

    /* Only use result if it expanded successfully to exactly one "word" and the result is a directory */
    if (result == 0) {
      char* to_return;
      if (w.we_wordc != 1)
        to_return = NULL;
      else if (g_file_test (w.we_wordv[0], G_FILE_TEST_IS_DIR))
        to_return = g_strdup (w.we_wordv[0]);
      else
        to_return = NULL;

      wordfree (&w);
      if (to_return)
        return to_return;
    }
  }
#endif

  if (g_file_test (str, G_FILE_TEST_IS_DIR))
    return g_strdup (str);
  else
    return NULL;
}



/* vim:set expandtab sts=2 ts=2 sw=2: */
