/***************************************************************************
 *            verve.h
 *
 *  Copyright  2007  Jannis Pohlmann
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
 
#ifndef __VERVE_H__
#define __VERVE_H__

#include "verve-env.h"
#include "verve-history.h"

/* Init / Shutdown Verve */
void verve_init (void);
void verve_shutdown (void);

/* Command line methods */
gboolean verve_spawn_command_line (const gchar *cmdline);
gboolean verve_execute (const gchar *input, gboolean terminal);

#endif /* !__VERVE_H__ */

/* vim:set expandtab ts=1 sw=2: */
