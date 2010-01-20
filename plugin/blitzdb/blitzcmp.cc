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

#include "ha_blitz.h"

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
    KEY *pk = &table->key_info[table->s->primary_key];
    KEY_PART_INFO *key_part = pk->key_part;
    KEY_PART_INFO *key_part_end = key_part + pk->key_parts;
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
      key_len = pack_index_key_from_row(key, table->s->primary_key, new_row);
      fetched = share->dict.get_row(key, key_len, &fetched_len);

      /* Key Exists. It's a violation. */
      if (fetched != NULL) {
        free(fetched);
        this->errkey_id = table->s->primary_key;
        return HA_ERR_FOUND_DUPP_KEY;
      }
    }
  }

  return 0;
}
