/*
 -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

 *  Definitions required for Query Cache plugin 

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

#ifndef DRIZZLED_PLUGIN_QCACHE_H
#define DRIZZLED_PLUGIN_QCACHE_H

typedef struct qcache_st
{
  /* todo, define this api */
  /* this is the API that a qcache plugin must implement.
     it should implement each of these function pointers.
     if a function returns bool true, that means it failed.
     if a function pointer is NULL, that's ok.
  */

  bool (*qcache_func1)(Session *thd, void *parm1, void *parm2);
  bool (*qcache_func2)(Session *thd, void *parm3, void *parm4);
} qcache_t;

#endif /* DRIZZLED_PLUGIN_QCACHE_H */
