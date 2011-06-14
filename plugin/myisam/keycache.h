/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/* Key cache variable structures */

#pragma once

enum flush_type
{
  FLUSH_KEEP,           /* flush block and keep it in the cache */
  FLUSH_RELEASE,        /* flush block and remove it from the cache */
  FLUSH_IGNORE_CHANGED, /* remove block from the cache */
  /*
 *     As my_disable_flush_pagecache_blocks is always 0, the following option
 *         is strictly equivalent to FLUSH_KEEP
 *           */
  FLUSH_FORCE_WRITE
};


/* declare structures that is used by st_key_cache */

struct st_block_link;
typedef struct st_block_link BLOCK_LINK;
struct st_keycache_page;
typedef struct st_keycache_page KEYCACHE_PAGE;
struct st_hash_link;
typedef struct st_hash_link HASH_LINK;

namespace drizzled
{
namespace internal
{
typedef uint64_t my_off_t;
struct st_my_thread_var;
}

/* info about requests in a waiting queue */
typedef struct st_keycache_wqueue
{
  drizzled::internal::st_my_thread_var *last_thread;  /* circular list of waiting threads */
} KEYCACHE_WQUEUE;

#define CHANGED_BLOCKS_HASH 128             /* must be power of 2 */

/*
  The key cache structure
  It also contains read-only statistics parameters.
*/

typedef struct st_key_cache
{
  bool key_cache_inited;
  bool can_be_used;           /* usage of cache for read/write is allowed */

  uint32_t key_cache_block_size;     /* size of the page buffer of a cache block */

  int blocks;                   /* max number of blocks in the cache        */

  bool in_init;		/* Set to 1 in MySQL during init/resize     */

  st_key_cache():
    key_cache_inited(false),
    can_be_used(false),
    key_cache_block_size(0),
    blocks(0),
    in_init(0)
  { }

} KEY_CACHE;

} /* namespace drizzled */

/* The default key cache */
extern int init_key_cache(drizzled::KEY_CACHE *keycache, uint32_t key_cache_block_size,
			  size_t use_mem, uint32_t division_limit,
			  uint32_t age_threshold);
extern unsigned char *key_cache_read(drizzled::KEY_CACHE *keycache,
                            int file, drizzled::internal::my_off_t filepos, int level,
                            unsigned char *buff, uint32_t length,
			    uint32_t block_length,int return_buffer);
extern int key_cache_insert(drizzled::KEY_CACHE *keycache,
                            int file, drizzled::internal::my_off_t filepos, int level,
                            unsigned char *buff, uint32_t length);
extern int key_cache_write(drizzled::KEY_CACHE *keycache,
                           int file, drizzled::internal::my_off_t filepos, int level,
                           unsigned char *buff, uint32_t length,
			   uint32_t block_length,int force_write);
extern int flush_key_blocks(drizzled::KEY_CACHE *keycache,
                            int file, enum flush_type type);
extern void end_key_cache(drizzled::KEY_CACHE *keycache, bool cleanup);

/*
  Next highest power of two

  SYNOPSIS
    my_round_up_to_next_power()
    v		Value to check

  RETURN
    Next or equal power of 2
    Note: 0 will return 0

  NOTES
    Algorithm by Sean Anderson, according to:
    http://graphics.stanford.edu/~seander/bithacks.html
    (Orignal code public domain)

    Comments shows how this works with 01100000000000000000000000001011
*/

static inline uint32_t my_round_up_to_next_power(uint32_t v)
{
  v--;			/* 01100000000000000000000000001010 */
  v|= v >> 1;		/* 01110000000000000000000000001111 */
  v|= v >> 2;		/* 01111100000000000000000000001111 */
  v|= v >> 4;		/* 01111111110000000000000000001111 */
  v|= v >> 8;		/* 01111111111111111100000000001111 */
  v|= v >> 16;		/* 01111111111111111111111111111111 */
  return v+1;		/* 10000000000000000000000000000000 */
}


