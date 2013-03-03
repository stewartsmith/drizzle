/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2013 Stewart Smith
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

#include <drizzled/message/table.pb.h>

namespace drizzled {

/* This structure describes a key part.
   There is one key part per field in a key.
 */
class KeyPartInfo
{
public:
  Field *field;
  unsigned int	offset;	     /* offset in record (from 0) */
  unsigned int	null_offset; /* Offset to null_bit in record */
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
  uint16_t fieldnr;		/* Fieldnum in UNIREG (1,2,3,...) */
  uint16_t key_part_flag;	/* 0 or HA_REVERSE_SORT */
  uint8_t type;
  uint8_t null_bit;		/* Position to null_bit */
};

} /* namespace drizzled */
