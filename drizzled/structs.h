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

namespace drizzled {

class KeyPartInfo 
{	/* Info about a key part */
public:
  Field *field;
  unsigned int	offset;				/* offset in record (from 0) */
  unsigned int	null_offset;			/* Offset to null_bit in record */
  /* Length of key part in bytes, excluding NULL flag and length bytes */
  uint16_t length;
  /*
    Number of bytes required to store the keypart value. This may be
    different from the "length" field as it also counts
     - possible NULL-flag byte (see HA_KEY_NULL_LENGTH) [if null_bit != 0,
       the first byte stored at offset is 1 if null, 0 if non-null; the
       actual value is stored from offset+1].
     - possible HA_KEY_BLOB_LENGTH bytes needed to store actual value length.
  */
  uint16_t store_length;
  uint16_t key_type;
private:
public:
  uint16_t getKeyType() const
  {
    return key_type;
  }
  uint16_t fieldnr;			/* Fieldnum in UNIREG (1,2,3,...) */
  uint16_t key_part_flag;			/* 0 or HA_REVERSE_SORT */
  uint8_t type;
  uint8_t null_bit;			/* Position to null_bit */
};


class KeyInfo 
{
public:
  unsigned int	key_length;		/* Tot length of key */
  enum  ha_key_alg algorithm;
  unsigned long flags;			/* dupp key and pack flags */
  unsigned int key_parts;		/* How many key_parts */
  uint32_t  extra_length;
  unsigned int usable_key_parts;	/* Should normally be = key_parts */
  uint32_t  block_size;
  KeyPartInfo *key_part;
  char	*name;				/* Name of key */
  /*
    Array of AVG(#records with the same field value) for 1st ... Nth key part.
    0 means 'not known'.
    For temporary heap tables this member is NULL.
  */
  ulong *rec_per_key;
  Table *table;
  LEX_STRING comment;
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

typedef int *(*update_var)(Session *, struct drizzle_show_var *);

} /* namespace drizzled */

	/* Bits in form->status */
#define STATUS_NO_RECORD	(1+2)	/* Record isn't usably */
#define STATUS_GARBAGE		1
#define STATUS_NOT_FOUND	2	/* No record in database when needed */
#define STATUS_NO_PARENT	4	/* Parent record wasn't found */
#define STATUS_NULL_ROW		32	/* table->null_row is set */

