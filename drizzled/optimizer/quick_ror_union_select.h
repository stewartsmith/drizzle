/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
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

#ifndef DRIZZLED_OPTIMIZER_QUICK_ROR_UNION_SELECT_H
#define DRIZZLED_OPTIMIZER_QUICK_ROR_UNION_SELECT_H

#include "drizzled/optimizer/range.h"

namespace drizzled
{

namespace optimizer
{

/*
 * This function object is defined in drizzled/optimizer/range.cc
 * We need this here for the priority_queue definition in the
 * QUICK_ROR_UNION_SELECT class.
 */
class compare_functor;

/**
  Rowid-Ordered Retrieval index union select.
  This quick select produces union of row sequences returned by several
  quick select it "merges".

  All merged quick selects must return rowids in rowid order.
  QUICK_ROR_UNION_SELECT will return rows in rowid order, too.

  All merged quick selects are set not to retrieve full table records.
  ROR-union quick select always retrieves full records.

*/
class QUICK_ROR_UNION_SELECT : public QuickSelectInterface
{
public:
  QUICK_ROR_UNION_SELECT(Session *session, Table *table);
  ~QUICK_ROR_UNION_SELECT();

  int  init();
  int  reset(void);
  int  get_next();

  bool reverse_sorted() const
  {
    return false;
  }

  bool unique_key_range() const
  {
    return false;
  }

  int get_type() const
  {
    return QS_TYPE_ROR_UNION;
  }

  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
  bool is_keys_used(const MyBitmap *fields);

  bool push_quick_back(QuickSelectInterface *quick_sel_range);

  List<QuickSelectInterface> quick_selects; /**< Merged quick selects */

  /** Priority queue for merge operation */
  std::priority_queue<QuickSelectInterface *, std::vector<QuickSelectInterface *>, compare_functor > *queue;
  MEM_ROOT alloc; /**< Memory pool for this and merged quick selects data. */

  Session *session; /**< current thread */
  unsigned char *cur_rowid; /**< buffer used in get_next() */
  unsigned char *prev_rowid; /**< rowid of last row returned by get_next() */
  bool have_prev_rowid; /**< true if prev_rowid has valid data */
  uint32_t rowid_length; /**< table rowid length */
private:
  bool scans_inited;
};

} /* namespace optimizer */

} /* namespace drizzled */

#endif /* DRIZZLED_OPTIMIZER_QUICK_ROR_UNION_SELECT_H */
