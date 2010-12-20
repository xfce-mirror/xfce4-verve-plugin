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

#ifndef __VERVE_HISTORY_H__
#define __VERVE_HISTORY_H__

#include <glib-object.h>

/* Init / Shutdown history database */
void         verve_history_init             (void);
void         verve_history_shutdown         (void);

void         verve_history_set_length       (gint         length);
void         verve_history_add              (gchar       *input);
GList       *verve_history_begin            (void);
GList       *verve_history_end              (void);
GList       *verve_history_get_prev         (const GList *current);
GList       *verve_history_get_next         (const GList *current);
gboolean     verve_history_is_empty         (void);
const gchar *verve_history_get_last_command (void);

#endif /* !__VERVE_HISTORY_H__ */

/* vim:set expandtab sts=2 ts=2 sw=2: */
