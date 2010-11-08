/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 - 2010 Toru Maesaka
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

#include "config.h"
#include "ha_blitz.h"

using namespace drizzled;

/* Given two native rows, this function checks all unique fields to
   find whether the value has been updated. If a unique field value
   is updated, it checks if the new key exists in the database already.
   If it already exists, then it is a unique constraint violation. */
int ha_blitz::compare_rows_for_unique_violation(const unsigned char *old_row,
                                                const unsigned char *new_row) {
  const unsigned char *old_pos, *new_pos;
  char *key, *fetched;
  int key_len, fetched_len;

  /* For now, we are only interested in supporting a PRIMARY KEY. In the
     next phase of BlitzDB, this should loop through the key array. */
  if (share->primary_key_exists) {
    KeyInfo *pk = &getTable()->key_info[getTable()->getShare()->getPrimaryKey()];
    KeyPartInfo *key_part = pk->key_part;
    KeyPartInfo *key_part_end = key_part + pk->key_parts;
    int key_changed = 0;

    while (key_part != key_part_end) {  
      old_pos = old_row + key_part->offset;
      new_pos = new_row + key_part->offset;

      /* In VARCHAR, first 2 bytes is reserved to represent the length
         of the key. Rip it out and move forward the row pointers. */
      if (key_part->type == HA_KEYTYPE_VARTEXT2) {
        uint16_t old_key_len = uint2korr(old_pos);
        uint16_t new_key_len = uint2korr(new_pos);
        old_pos += sizeof(old_key_len);
        new_pos += sizeof(new_key_len);

        /* Compare the actual data by repecting it's collation. */
        key_changed = my_strnncoll(&my_charset_utf8_general_ci, old_pos,
                                   old_key_len, new_pos, new_key_len);
      } else if (key_part->type == HA_KEYTYPE_BINARY) {
        return HA_ERR_UNSUPPORTED;
      } else {
        key_changed = memcmp(old_pos, new_pos, key_part->length);
      }
      key_part++;
    }

    /* Key has changed. Look up the database to see if the new value
       would violate the unique contraint. */
    if (key_changed) {
      key = key_buffer;
      key_len = make_index_key(key, getTable()->getMutableShare()->getPrimaryKey(), new_row);
      fetched = share->dict.get_row(key, key_len, &fetched_len);

      /* Key Exists. It's a violation. */
      if (fetched != NULL) {
        free(fetched);
        this->errkey_id = getTable()->getShare()->getPrimaryKey();
        return HA_ERR_FOUND_DUPP_KEY;
      }
    }
  }

  return 0;
}

/* 
 * Comparison Function for TC's B+Tree. This is the only function
 * in BlitzDB where you can feel free to write redundant code for
 * performance. Avoid function calls unless it is necessary. If
 * you reaaaally want to separate the code block for readability,
 * make sure to separate the code with C++ template for inlining.
 *                                               - Toru
 * Currently Handles:
 *   INT       (interpreted as LONG_INT)
 *   BIGINT    (interpreted as LONGLONG)
 *   TIMESTAMP (interpreted as ULONG_INT)
 *   DATETIME  (interpreted as ULONGLONG)
 *   DATE      (interpreted as UINT_24)
 *   DOUBLE    (interpreted as DOUBLE)
 *   VARCHAR   (VARTEXT1 or VARTEXT2)
 *
 * BlitzDB Key Format:
 *   [DRIZZLE_KEY][UINT16_T][BLITZDB_UNIQUE_KEY]
 *
 * Returns:
 *   -1 = a < b
 *    0 = a == b
 *    1 = a > b
 */
int blitz_keycmp_cb(const char *a, int,
                    const char *b, int, void *opaque) {
  BlitzTree *tree = (BlitzTree *)opaque;
  int a_compared_len, b_compared_len, rv;

  rv = packed_key_cmp(tree, a, b, &a_compared_len, &b_compared_len);

  if (rv != 0)
    return rv;

  /* Getting here means that the Drizzle part of the keys are
     identical. We now compare the BlitzDB part of the key. */
  if (tree->unique) {
    /* This is a special exception that allows Drizzle's NULL
       key to be inserted duplicately into a UNIQUE tree. If
       the keys aren't NULL then it is safe to conclude that
       the keys are identical which this condition does. */
    if (*a != 0 && *b != 0)
      return 0;
  }

  char *a_pos = (char *)a + a_compared_len;
  char *b_pos = (char *)b + b_compared_len;

  uint16_t a_pk_len = uint2korr(a_pos);
  uint16_t b_pk_len = uint2korr(b_pos);

  a_pos += sizeof(a_pk_len);
  b_pos += sizeof(b_pk_len);

  if (a_pk_len == b_pk_len)
    rv = memcmp(a_pos, b_pos, a_pk_len);
  else if (a_pk_len < b_pk_len)
    rv = -1;
  else
    rv = 1;

  return rv;
}

/* General purpose comparison function for BlitzDB. We cannot reuse
   blitz_keycmp_cb() for this purpose because the 'exact match' only
   applies to BlitzDB's unique B+Tree key format in blitz_keycmp_cb().
   Here we are comparing packed Drizzle keys. */
int packed_key_cmp(BlitzTree *tree, const char *a, const char *b,
                   int *a_compared_len, int *b_compared_len) {
  BlitzKeyPart *curr_part;
  char *a_pos = (char *)a;
  char *b_pos = (char *)b;
  int a_next_offset = 0;
  int b_next_offset = 0;

  *a_compared_len = 0;
  *b_compared_len = 0;

  for (int i = 0; i < tree->nparts; i++) {
    curr_part = &tree->parts[i];

    if (curr_part->null_bitmask) {
      *a_compared_len += 1;
      *b_compared_len += 1;

      if (*a_pos != *b_pos)
        return ((int)*a_pos - (int)*b_pos);

      b_pos++;

      if (!*a_pos++) {
        curr_part++;
        continue;
      }
    }

    switch ((enum ha_base_keytype)curr_part->type) {
    case HA_KEYTYPE_LONG_INT: {
      int32_t a_int_val = sint4korr(a_pos);
      int32_t b_int_val = sint4korr(b_pos);

      *a_compared_len += curr_part->length;
      *b_compared_len += curr_part->length;

      if (a_int_val < b_int_val)
        return -1;
      else if (a_int_val > b_int_val)
        return 1;

      a_next_offset = b_next_offset = curr_part->length;
      break; 
    }
    case HA_KEYTYPE_ULONG_INT: {
      uint32_t a_int_val = uint4korr(a_pos);
      uint32_t b_int_val = uint4korr(b_pos);

      *a_compared_len += curr_part->length;
      *b_compared_len += curr_part->length;

      if (a_int_val < b_int_val)
        return -1;
      else if (a_int_val > b_int_val)
        return 1;

      a_next_offset = b_next_offset = curr_part->length;
      break;
    }
    case HA_KEYTYPE_LONGLONG: {
      int64_t a_int_val = sint8korr(a_pos);
      int64_t b_int_val = sint8korr(b_pos);

      *a_compared_len += curr_part->length;
      *b_compared_len += curr_part->length;

      if (a_int_val < b_int_val)
        return -1;
      else if (a_int_val > b_int_val)
        return 1;

      a_next_offset = b_next_offset = curr_part->length;
      break;
    }
    case HA_KEYTYPE_ULONGLONG: {
      uint64_t a_int_val = uint8korr(a_pos);
      uint64_t b_int_val = uint8korr(b_pos);

      *a_compared_len += curr_part->length;
      *b_compared_len += curr_part->length;

      if (a_int_val < b_int_val)
        return -1;
      else if (a_int_val > b_int_val)
        return 1;

      a_next_offset = b_next_offset = curr_part->length;
      break;
    }
    case HA_KEYTYPE_DOUBLE: {
      double a_double_val, b_double_val;
      float8get(a_double_val, a_pos);
      float8get(b_double_val, b_pos);

      *a_compared_len += curr_part->length;
      *b_compared_len += curr_part->length;

      if (a_double_val < b_double_val)
        return -1;
      else if (a_double_val > b_double_val)
        return 1;

      a_next_offset = b_next_offset = curr_part->length;
      break;
    }
    case HA_KEYTYPE_VARTEXT1:
    case HA_KEYTYPE_VARTEXT2: {
      uint16_t a_varchar_len = uint2korr(a_pos);
      uint16_t b_varchar_len = uint2korr(b_pos);
      int key_changed;

      a_pos += sizeof(a_varchar_len);
      b_pos += sizeof(b_varchar_len);

      *a_compared_len += a_varchar_len + sizeof(a_varchar_len);
      *b_compared_len += b_varchar_len + sizeof(b_varchar_len);

      /* Compare the texts by respecting collation. */
      key_changed = my_strnncoll(&my_charset_utf8_general_ci,
                                 (unsigned char *)a_pos, a_varchar_len,
                                 (unsigned char *)b_pos, b_varchar_len);
      if (key_changed < 0)
        return -1;
      else if (key_changed > 0)
        return 1;

      a_next_offset = a_varchar_len;
      b_next_offset = b_varchar_len;
      break; 
    }
    default:
      break;
    }

    a_pos += a_next_offset;
    b_pos += b_next_offset;
    curr_part++;
  }

  return 0;
}
