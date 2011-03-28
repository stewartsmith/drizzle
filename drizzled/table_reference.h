/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *
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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file
 *
 * Defines the JoinTable class which is the primary class
 * used in the nested loops join implementation.
 */

#pragma once

#include <drizzled/base.h>
#include <drizzled/definitions.h>

namespace drizzled {

struct table_reference_st
{
  table_reference_st() :
    key_err(false),
    key_parts(0),
    key_length(0),
    key(0),
    key_buff(NULL),
    key_buff2(NULL),
    key_copy(NULL),
    items(NULL),
    cond_guards(NULL),
    null_ref_key(NULL),
    disable_cache(false)
  { }

  bool key_err;
  uint32_t key_parts; /**< num of key parts */
  uint32_t key_length; /**< length of key_buff */
  int32_t key; /**< key no (index) */
  unsigned char *key_buff; /**< value to look for with key */
  unsigned char *key_buff2; /**< key_buff+key_length */
  StoredKey **key_copy; /**< No idea what this does... */
  Item **items; /**< val()'s for each keypart */
  /**
    Array of pointers to trigger variables. Some/all of the pointers may be
    NULL.  The ref access can be used iff

      for each used key part i, (!cond_guards[i] || *cond_guards[i])

    This array is used by subquery code. The subquery code may inject
    triggered conditions, i.e. conditions that can be 'switched off'. A ref
    access created from such condition is not valid when at least one of the
    underlying conditions is switched off (see subquery code for more details)
  */
  bool **cond_guards;
  /**
    (null_rejecting & (1<<i)) means the condition is '=' and no matching
    rows will be produced if items[i] IS NULL (see add_not_null_conds())
  */
  key_part_map  null_rejecting;
  table_map	depend_map; /**< Table depends on these tables. */
  /** null byte position in the key_buf. Used for REF_OR_NULL optimization */
  unsigned char *null_ref_key;
  /**
    true <=> disable the "cache" as doing lookup with the same key value may
    produce different results (because of Index Condition Pushdown)
  */
  bool disable_cache;
};

} /* namespace drizzled */

