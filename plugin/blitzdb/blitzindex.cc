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
    return drizzled::HA_ERR_OUT_OF_MEM;

  if (!tcbdbsetmutex(btree)) {
    tcbdbdel(btree);
    return drizzled::HA_ERR_GENERIC;
  }

  if (blitz_estimated_rows != 0) {
    uint64_t tree_buckets = blitz_estimated_rows / 10;
    if (!tcbdbtune(btree, 0, 0, tree_buckets, -1, -1, 0)) {
      tcbdbdel(btree);
      return drizzled::HA_ERR_GENERIC;
    }
  }

  if (!tcbdbsetcmpfunc(btree, blitz_keycmp_cb, this)) {
    tcbdbdel(btree);
    return drizzled::HA_ERR_GENERIC;
  }

  snprintf(buf, FN_REFLEN, "%s_%02d%s", path, key_num, BLITZ_INDEX_EXT);

  if (!tcbdbopen(btree, buf, mode)) {
    tcbdbdel(btree);
    return drizzled::HA_ERR_CRASHED_ON_USAGE;
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

  return drizzled::internal::my_rename(from_buf, to_buf, MYF(0));
}

int BlitzTree::close(void) {
  assert(btree);

  if (!tcbdbclose(btree)) {
    tcbdbdel(btree);
    return drizzled::HA_ERR_CRASHED_ON_USAGE;
  }

  tcbdbdel(btree);
  return 0;
}

bool BlitzTree::create_cursor(BlitzCursor *bc) {
  if (bc == NULL)
    return false;

  if ((bc->cursor = tcbdbcurnew(btree)) == NULL)
    return false;

  bc->tree = this;
  bc->active = true;
  bc->moved = false;
  return true;
}

void BlitzTree::destroy_cursor(BlitzCursor *bc) {
  if (bc == NULL)
    return;

  tcbdbcurdel(bc->cursor);
  return;
}

int BlitzTree::write(const char *key, const size_t klen) {
  return (tcbdbputdup(btree, key, klen, "", 0)) ? 0 : -1;
}

int BlitzTree::write_unique(const char *key, const size_t klen) {
  if (!tcbdbputkeep(btree, key, klen, "", 0)) {
    if (tcbdbecode(btree) == TCEKEEP) {
      errno = drizzled::HA_ERR_FOUND_DUPP_KEY;
      return drizzled::HA_ERR_FOUND_DUPP_KEY;
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
