/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#pragma once

#include <drizzled/base.h>
#include <drizzled/definitions.h>
#include <drizzled/lex_string.h>
#include <drizzled/thr_lock.h>
#include <drizzled/message/table.pb.h>

namespace drizzled {

class KeyPartInfo;

class KeyInfo
{
public:
  unsigned int	key_length;		/* Tot length of key */
  drizzled::message::Table::Index::IndexType algorithm;
  unsigned long flags;			/* dupp key and pack flags */
  unsigned int key_parts;		/* How many key_parts */
  uint32_t  extra_length;
  unsigned int usable_key_parts;	/* Should normally be = key_parts */
  uint32_t  block_size;
  KeyPartInfo *key_part;
  const char* name;				/* Name of key */
  /*
    Array of AVG(#records with the same field value) for 1st ... Nth key part.
    0 means 'not known'.
    For temporary heap tables this member is NULL.
  */
  ulong *rec_per_key;
  Table *table;
  str_ref comment;
};


class RegInfo 
{
public:		/* Extra info about reg */
  JoinTable *join_tab;	/* Used by SELECT() */
  enum thr_lock_type lock_type;		/* How database is used */
  bool not_exists_optimize;
  bool impossible_range;
  RegInfo()
    : join_tab(NULL), lock_type(TL_UNLOCK),
      not_exists_optimize(false), impossible_range(false) {}
  void reset()
  {
    join_tab= NULL;
    lock_type= TL_UNLOCK;
    not_exists_optimize= false;
    impossible_range= false;
  }
};

} /* namespace drizzled */

	/* Bits in form->status */
#define STATUS_NO_RECORD	(1+2)	/* Record isn't usably */
#define STATUS_GARBAGE		1
#define STATUS_NOT_FOUND	2	/* No record in database when needed */
#define STATUS_NO_PARENT	4	/* Parent record wasn't found */
#define STATUS_NULL_ROW		32	/* table->null_row is set */

