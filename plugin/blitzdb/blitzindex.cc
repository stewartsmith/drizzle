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

#include <config.h>
#include "ha_blitz.h"

/* Unlike the data dictionary, don't tune the btree by default
   since the default configuration satisfies BlitzDB's default
   performance requirements. Tuning parameters will be made dynamic
   in the upcoming releases. */
int BlitzTree::open(const char *path, const int key_num, int mode) {
  char buf[FN_REFLEN];

  if ((btree = tcbdbnew()) == NULL)
    return HA_ERR_OUT_OF_MEM;

  if (!tcbdbsetmutex(btree)) {
    tcbdbdel(btree);
    return HA_ERR_CRASHED_ON_USAGE;
  }

  if (!tcbdbsetcmpfunc(btree, blitz_keycmp_cb, this)) {
    tcbdbdel(btree);
    return HA_ERR_CRASHED_ON_USAGE;
  }

  snprintf(buf, FN_REFLEN, "%s_%02d%s", path, key_num, BLITZ_INDEX_EXT);

  if (!tcbdbopen(btree, buf, mode)) {
    tcbdbdel(btree);
    return HA_ERR_CRASHED_ON_USAGE;
  }

  return 0;
}

/* Similar to UNIX touch(1) but generates a TCBDB file. */
int BlitzTree::create(const char *path, const int key_num) {
  int rv;

  if ((rv = this->open(path, key_num, (BDBOWRITER | BDBOCREAT))) != 0)
    return rv;

  if ((rv = this->close()) != 0)
    return rv;

  return rv;
}

int BlitzTree::drop(const char *path, const int key_num) {
  char buf[FN_REFLEN];
  snprintf(buf, FN_REFLEN, "%s_%02d%s", path, key_num, BLITZ_INDEX_EXT);
  return unlink(buf);
}

int BlitzTree::rename(const char *from, const char *to, const int key_num) {
  char from_buf[FN_REFLEN];
  char to_buf[FN_REFLEN];

  snprintf(from_buf, FN_REFLEN, "%s_%02d%s", from, key_num, BLITZ_INDEX_EXT);
  snprintf(to_buf, FN_REFLEN, "%s_%02d%s", to, key_num, BLITZ_INDEX_EXT);

  return std::rename(from_buf, to_buf);
}

int BlitzTree::close(void) {
  assert(btree);

  if (!tcbdbclose(btree)) {
    tcbdbdel(btree);
    return HA_ERR_CRASHED_ON_USAGE;
  }

  tcbdbdel(btree);
  return 0;
}

BlitzCursor *BlitzTree::create_cursor(void) {
  /* Resort instantiating a new object everytime for now.
     In the future this will be a singleton for each Cursor
     object. We must first confirm it's safety. */
  BlitzCursor *bc = new BlitzCursor();

  if ((bc->cursor = tcbdbcurnew(btree)) == NULL)
    return NULL;

  bc->tree = this;
  bc->active = true;
  bc->moved = false;
  return bc;
}

void BlitzTree::destroy_cursor(BlitzCursor *bc) {
  if (bc == NULL)
    return;

  tcbdbcurdel(bc->cursor);
  delete bc; 
}

int BlitzTree::write(const char *key, const size_t klen) {
  return (tcbdbputdup(btree, key, klen, "", 0)) ? 0 : -1;
}

int BlitzTree::write_unique(const char *key, const size_t klen) {
  if (!tcbdbputkeep(btree, key, klen, "", 0)) {
    if (tcbdbecode(btree) == TCEKEEP) {
      errno = HA_ERR_FOUND_DUPP_KEY;
      return HA_ERR_FOUND_DUPP_KEY;
    }
  }
  return 0;
}

int BlitzTree::delete_key(const char *key, const int klen) {
  return (tcbdbout(btree, key, klen)) ? 0 : -1;
}

int BlitzTree::delete_all(void) {
  return (tcbdbvanish(btree)) ? 0 : -1;
}

uint64_t BlitzTree::records(void) {
  return tcbdbrnum(btree);
}

/* == BlitzCursor Code From Here == */

char *BlitzCursor::first_key(int *key_len) {
  if (!tcbdbcurfirst(cursor))
    return NULL;

  return (char *)tcbdbcurkey(cursor, key_len);
}

char *BlitzCursor::final_key(int *key_len) {
  if (!tcbdbcurlast(cursor))
    return NULL;

  return (char *)tcbdbcurkey(cursor, key_len);
}

/* It's possible that the cursor had been implicitly moved
   forward. If so, we obtain the key at the current position
   and return that. This can happen in delete_key(). */
char *BlitzCursor::next_key(int *key_len) {
  if (!moved) {
    if (!tcbdbcurnext(cursor))
      return NULL;
  } else
    moved = false;

  return (char *)tcbdbcurkey(cursor, key_len);
}

char *BlitzCursor::prev_key(int *key_len) {
  if (!tcbdbcurprev(cursor))
    return NULL;

  return (char *)tcbdbcurkey(cursor, key_len);
}

char *BlitzCursor::next_logical_key(int *key_len) {
  int origin_len, a_cmp_len, b_cmp_len;
  char *rv, *origin;

  /* Get the key to scan away from. */
  if ((origin = (char *)tcbdbcurkey(cursor, &origin_len)) == NULL)
    return NULL;

  rv = NULL;

  /* Traverse the tree from the cursor position until we hit
     a key greater than the origin or EOF. */
  while (1) {
    /* Move the cursor and get the next key. */
    if (!tcbdbcurnext(cursor))
      break;
  
    rv = (char *)tcbdbcurkey(cursor, key_len);

    /* Compare the fetched key with origin. */
    if (packed_key_cmp(this->tree, rv, origin, &a_cmp_len, &b_cmp_len) > 0)
      break;

    free(rv);
  }

  free(origin);
  return rv;
}

char *BlitzCursor::prev_logical_key(int *key_len) {
  int origin_len, a_cmp_len, b_cmp_len;
  char *rv, *origin;

  /* Get the key to scan away from. */
  if ((origin = (char *)tcbdbcurkey(cursor, &origin_len)) == NULL)
    return NULL;

  rv = NULL;

  while (1) {
    if (!tcbdbcurprev(cursor))
      break;
  
    rv = (char *)tcbdbcurkey(cursor, key_len);

    /* Compare the fetched key with origin. */
    if (packed_key_cmp(this->tree, rv, origin, &a_cmp_len, &b_cmp_len) < 0)
      break;

    free(rv);
  }

  free(origin);
  return rv;
}

/* A cursor based lookup on a B+Tree doesn't guarantee that the
   returned key is identical. This is because it would return the
   next logical key if one exists. */
char *BlitzCursor::find_key(const int search_mode, const char *key,
                            const int klen, int *rv_len) {

  if (!tcbdbcurjump(cursor, (void *)key, klen))
    return NULL;

  char *rv = (char *)tcbdbcurkey(cursor, rv_len);

  if (rv == NULL)
    return NULL;

  int cmp, a_cmp_len, b_cmp_len;
  cmp = packed_key_cmp(this->tree, rv, key, &a_cmp_len, &b_cmp_len);

  switch (search_mode) {
  case drizzled::HA_READ_KEY_EXACT:
    if (cmp != 0) {
      free(rv);
      rv = NULL;
    }
    break;
  case drizzled::HA_READ_AFTER_KEY:
    if (cmp > 0) {
      break;
    } else if (cmp == 0) {
      free(rv);
      rv = this->next_logical_key(rv_len);
    } else {
      free(rv);
      rv = NULL;
    }
    break;
  case drizzled::HA_READ_BEFORE_KEY:
    free(rv);
    rv = this->prev_logical_key(rv_len);
    break;
  //case drizzled::HA_READ_KEY_OR_NEXT:
  //case drizzled::HA_READ_KEY_OR_PREV:
  //case drizzled::HA_READ_PREFIX:
  //case drizzled::HA_READ_PREFIX_LAST:
  //case drizzled::HA_READ_PREFIX_LAST_OR_PREV:
  default:
     break;
  }

  return rv;
}

int BlitzCursor::delete_position(void) {
  if (!tcbdbcurout(cursor))
    return -1;

  moved = true;
  return 0;
}
