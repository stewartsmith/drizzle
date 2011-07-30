/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

/* Dynamic hashing of record with different key-length */

#pragma once

#include <drizzled/dynamic_array.h>

namespace drizzled {

/*
  Overhead to store an element in hash
  Can be used to approximate memory consumption for a hash
 */
#define HASH_OVERHEAD (sizeof(char*)*2)

/* flags for hash_init */
#define HASH_UNIQUE     1       /* hash_insert fails on duplicate key */

typedef unsigned char *(*hash_get_key)(const unsigned char *,size_t*,bool);
typedef void (*hash_free_key)(void *);

struct HASH_LINK 
{
  /* index to next key */
  uint32_t next;
  /* data for current entry */
  unsigned char *data;
} ;

struct charset_info_st;

struct HASH
{
  // typedef std::vector<HASH_LINK> array_t;
  typedef DYNAMIC_ARRAY array_t;
  /* Length of key if const length */
  size_t key_offset,key_length;
  uint32_t blength;
  uint32_t records;
  uint32_t flags;
  /* Place for hash_keys */
  array_t array;
  hash_get_key get_key;
  hash_free_key free;
  const charset_info_st *charset;
};

/* A search iterator state */
typedef uint32_t HASH_SEARCH_STATE;

void
_hash_init(HASH *hash,uint32_t growth_size, const charset_info_st* const,
           uint32_t size, size_t key_offset, size_t key_length,
           hash_get_key get_key,
           hash_free_key free_element, uint32_t flags);
#define hash_init(A,B,C,D,E,F,G,H) _hash_init(A,0,B,C,D,E,F,G,H)
void hash_free(HASH *tree);
unsigned char *hash_search(const HASH *info, const unsigned char *key,
                           size_t length);
unsigned char *hash_first(const HASH *info, const unsigned char *key, size_t length, HASH_SEARCH_STATE *state);
bool my_hash_insert(HASH *info,const unsigned char *data);
bool hash_delete(HASH *hash,unsigned char *record);

} /* namespace drizzled */

