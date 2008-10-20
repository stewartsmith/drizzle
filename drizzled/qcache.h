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

#ifndef DRIZZLED_QCACHE_H
#define DRIZZLED_QCACHE_H

#include <drizzled/plugin_qcache.h>

int qcache_initializer (st_plugin_int *plugin);
int qcache_finalizer (st_plugin_int *plugin);

/* todo, fill in this API */
/* these are the functions called by the rest of the drizzle server
   to do whatever this plugin does. */
bool qcache_do1 (Session *session, void *parm1, void *parm2);
bool qcache_do2 (Session *session, void *parm3, void *parm4);

#endif /* DRIZZLED_QCACHE_H */
