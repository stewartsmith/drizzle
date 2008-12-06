/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Mark Atwood
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef DRIZZLED_REPLICATOR_H
#define DRIZZLED_REPLICATOR_H

#include <drizzled/plugin_replicator.h>

int replicator_initializer (st_plugin_int *plugin);
int replicator_finalizer (st_plugin_int *plugin);

/* todo, fill in this API */
/* these are the functions called by the rest of the drizzle server
   to do whatever this plugin does. */
bool replicator_session_init (Session *session);
bool replicator_write_row(Session *session, Table *table);
bool replicator_update_row(Session *session, Table *table, 
                           const unsigned char *before, 
                           const unsigned char *after);
bool replicator_delete_row(Session *session, Table *table);

#endif /* DRIZZLED_REPLICATOR_H */
