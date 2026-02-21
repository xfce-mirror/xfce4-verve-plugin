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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "verve-completion.h"

#include <string.h>

static void verve_completion_check_cache (VerveCompletion* cmp,
                                          gchar**          new_prefix);


VerveCompletion* 
verve_completion_new (VerveCompletionFunc func)
{
  VerveCompletion* gcomp;
  
  gcomp = g_new (VerveCompletion, 1);
  gcomp->items = NULL;
  gcomp->cache = NULL;
  gcomp->prefix = NULL;
  gcomp->func = func;
  gcomp->strncmp_func = strncmp;

  return gcomp;
}


void 
verve_completion_add_items (VerveCompletion* cmp,
                            GList*	         items)
{
  GList* it;
  
  g_return_if_fail (cmp != NULL);
  
  /* optimize adding to cache? */
  if (cmp->cache)
    {
      g_list_free (cmp->cache);
      cmp->cache = NULL;
    }

  if (cmp->prefix)
    {
      g_free (cmp->prefix);
      cmp->prefix = NULL;
    }
  
  it = items;
  while (it)
    {
      cmp->items = g_list_prepend (cmp->items, it->data);
      it = it->next;
    }
}


void 
verve_completion_clear_items (VerveCompletion* cmp)
{
  g_return_if_fail (cmp != NULL);
  
  g_list_free (cmp->items);
  cmp->items = NULL;
  g_list_free (cmp->cache);
  cmp->cache = NULL;
  g_free (cmp->prefix);
  cmp->prefix = NULL;
}


static void   
verve_completion_check_cache (VerveCompletion* cmp,
                              gchar**	         new_prefix)
{
  GList* list;
  gsize len;
  gsize i;
  gsize plen;
  gchar* postfix;
  gchar* s;
  
  if (!new_prefix)
    return;
  if (!cmp->cache)
    {
      *new_prefix = NULL;
      return;
    }
  
  len = strlen(cmp->prefix);
  list = cmp->cache;
  s = cmp->func ? cmp->func (list->data) : (gchar*) list->data;
  postfix = s + len;
  plen = strlen (postfix);
  list = list->next;
  
  while (list && plen)
    {
      s = cmp->func ? cmp->func (list->data) : (gchar*) list->data;
      s += len;
      for (i = 0; i < plen; ++i)
  {
    if (postfix[i] != s[i])
      break;
  }
      plen = i;
      list = list->next;
    }
  
  *new_prefix = g_new0 (gchar, len + plen + 1);
  strncpy (*new_prefix, cmp->prefix, len);
  strncpy (*new_prefix + len, postfix, plen);
}


GList* 
verve_completion_complete (VerveCompletion* cmp,
                           const gchar*     prefix,
                           gchar**	        new_prefix)
{
  gsize plen, len;
  gboolean done = FALSE;
  GList* list;
  
  g_return_val_if_fail (cmp != NULL, NULL);
  g_return_val_if_fail (prefix != NULL, NULL);
  
  len = strlen (prefix);
  if (cmp->prefix && cmp->cache)
    {
      plen = strlen (cmp->prefix);
      if (plen <= len && ! cmp->strncmp_func (prefix, cmp->prefix, plen))
  { 
    /* use the cache */
    list = cmp->cache;
    while (list)
      {
        GList *next = list->next;
        
        if (cmp->strncmp_func (prefix,
             cmp->func ? cmp->func (list->data) : (gchar*) list->data,
             len))
    cmp->cache = g_list_delete_link (cmp->cache, list);

        list = next;
      }
    done = TRUE;
  }
    }
  
  if (!done)
    {
      /* normal code */
      g_list_free (cmp->cache);
      cmp->cache = NULL;
      list = cmp->items;
      while (*prefix && list)
  {
    if (!cmp->strncmp_func (prefix,
      cmp->func ? cmp->func (list->data) : (gchar*) list->data,
      len))
      cmp->cache = g_list_prepend (cmp->cache, list->data);
    list = list->next;
  }
    }
  if (cmp->prefix)
    {
      g_free (cmp->prefix);
      cmp->prefix = NULL;
    }
  if (cmp->cache)
    cmp->prefix = g_strdup (prefix);
  verve_completion_check_cache (cmp, new_prefix);
  
  return *prefix ? cmp->cache : cmp->items;
}


void 
verve_completion_free (VerveCompletion* cmp)
{
  g_return_if_fail (cmp != NULL);
  
  verve_completion_clear_items (cmp);
  g_free (cmp);
}
