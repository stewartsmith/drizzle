/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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


/* This struct includes all reserved words and functions */

#ifndef _lex_symbol_h
#define _lex_symbol_h

#include <mysys/thr_lock.h>
/* A helper type for transactional locking. */
struct st_table_lock_info
{
  thr_lock_type lock_type;
  int           lock_timeout;
  bool          lock_transactional;
};

struct st_sym_group;

typedef struct st_symbol {
  const char *name;
  uint	tok;
  uint length;
  struct st_sym_group *group;
} SYMBOL;

typedef struct st_lex_symbol
{
  SYMBOL *symbol;
  char   *str;
  uint   length;
} LEX_SYMBOL;

typedef struct st_sym_group {
  const char *name;
  const char *needed_define;
} SYM_GROUP;

extern SYM_GROUP sym_group_common;
extern SYM_GROUP sym_group_geom;
extern SYM_GROUP sym_group_rtree;

#endif /* _lex_symbol_h */
