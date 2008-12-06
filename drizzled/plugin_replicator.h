/*
  -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
  *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

  *  Definitions required for Configuration Variables plugin 

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

#ifndef DRIZZLED_PLUGIN_REPLICATOR_H
#define DRIZZLED_PLUGIN_REPLICATOR_H

typedef struct replicator_st
{
  bool enabled;
  /* todo, define this api */
  /* this is the API that a replicator plugin must implement.
     it should implement each of these function pointers.
     if a function returns bool true, that means it failed.
     if a function pointer is NULL, that's ok.
  */

  void *(*session_init)(Session *session);
  bool (*row_insert)(Session *session, Table *table);
  bool (*row_update)(Session *session, Table *table, 
                     const unsigned char *before, 
                     const unsigned char *after);
  bool (*row_delete)(Session *session, Table *table);
} replicator_t;

#endif /* DRIZZLED_PLUGIN_REPLICATOR_H */
