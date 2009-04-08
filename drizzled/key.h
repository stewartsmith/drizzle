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


#ifndef DRIZZLED_KEY_H
#define DRIZZLED_KEY_H


#include <drizzled/sql_alloc.h>
#include <drizzled/key_part_spec.h>
#include <drizzled/sql_list.h>
#include <drizzled/lex_string.h>
#include <drizzled/handler_structs.h>
#include <bitset>

class Item;
typedef struct st_mem_root MEM_ROOT;

class Key :public Sql_alloc {
public:
  enum Keytype { PRIMARY, UNIQUE, MULTIPLE, FOREIGN_KEY};
  enum Keytype type;
  KEY_CREATE_INFO key_create_info;
  List<Key_part_spec> columns;
  LEX_STRING name;
  bool generated;

  Key(enum Keytype type_par, const LEX_STRING &name_arg,
      KEY_CREATE_INFO *key_info_arg,
      bool generated_arg, List<Key_part_spec> &cols)
    :type(type_par), key_create_info(*key_info_arg), columns(cols),
    name(name_arg), generated(generated_arg)
  {}
  Key(enum Keytype type_par, const char *name_arg, size_t name_len_arg,
      KEY_CREATE_INFO *key_info_arg, bool generated_arg,
      List<Key_part_spec> &cols)
    :type(type_par), key_create_info(*key_info_arg), columns(cols),
    generated(generated_arg)
  {
    name.str= (char *)name_arg;
    name.length= name_len_arg;
  }

  /**
   * Construct an (almost) deep copy of this key. Only those
   * elements that are known to never change are not copied.
   * If out of memory, a partial copy is returned and an error is set
   * in Session.
   */
  Key(const Key &rhs, MEM_ROOT *mem_root);
  virtual ~Key() {}
  /* Equality comparison of keys (ignoring name) */
  friend bool foreign_key_prefix(Key *a, Key *b);
  /**
    Used to make a clone of this object for ALTER/CREATE TABLE
    @sa comment for Key_part_spec::clone
  */
  virtual Key *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Key(*this, mem_root); }
};


int find_ref_key(KEY *key, uint32_t key_count, unsigned char *record, Field *field,
                 uint32_t *key_length, uint32_t *keypart);
/**
  Copy part of a record that forms a key or key prefix to a buffer.

    The function takes a complete table record (as e.g. retrieved by
    handler::index_read()), and a description of an index on the same table,
    and extracts the first key_length bytes of the record which are part of a
    key into to_key. If length == 0 then copy all bytes from the record that
    form a key.

  @param to_key      buffer that will be used as a key
  @param from_record full record to be copied from
  @param key_info    descriptor of the index
  @param key_length  specifies length of all keyparts that will be copied
*/

void key_copy(unsigned char *to_key, unsigned char *from_record, KEY *key_info, uint32_t key_length);
void key_copy(std::basic_string<unsigned char> &to_key,
              unsigned char *from_record, KEY *key_info, uint32_t key_length);
void key_restore(unsigned char *to_record, unsigned char *from_key, KEY *key_info,
                 uint16_t key_length);
void key_zero_nulls(unsigned char *tuple, KEY *key_info);
bool key_cmp_if_same(Table *form,const unsigned char *key,uint32_t index,uint32_t key_length);
void key_unpack(String *to,Table *form,uint32_t index);
bool is_key_used(Table *table, uint32_t idx, const std::bitset<MAX_FIELDS> *fields);
int key_cmp(KEY_PART_INFO *key_part, const unsigned char *key, uint32_t key_length);
extern "C" int key_rec_cmp(void *key_info, unsigned char *a, unsigned char *b);
#endif /* DRIZZLED_KEY_H */
