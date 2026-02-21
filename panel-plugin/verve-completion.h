/* Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __VERVE_COMPLETION_H__
#define __VERVE_COMPLETION_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _VerveCompletion     VerveCompletion;

typedef gchar*          (*VerveCompletionFunc)      (gpointer item);

typedef gint (*VerveCompletionStrncmpFunc) (const gchar *s1,
                                        const gchar *s2,
                                        gsize        n);

struct _VerveCompletion
{
  GList* items;
  VerveCompletionFunc func;
 
  gchar* prefix;
  GList* cache;
  VerveCompletionStrncmpFunc strncmp_func;
};

VerveCompletion* verve_completion_new           (VerveCompletionFunc func);
void         verve_completion_add_items     (VerveCompletion*    cmp,
                                         GList*          items);
void         verve_completion_clear_items   (VerveCompletion*    cmp);
GList*       verve_completion_complete      (VerveCompletion*    cmp,
                                         const gchar*    prefix,
                                         gchar**         new_prefix);
void         verve_completion_free          (VerveCompletion*    cmp);
G_END_DECLS

#endif /* __VERVE_COMPLETION_H__ */
