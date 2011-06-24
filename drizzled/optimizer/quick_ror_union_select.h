/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#pragma once

#include <drizzled/optimizer/range.h>

#include <vector>

namespace drizzled {
namespace optimizer {

/**
  Rowid-Ordered Retrieval index union select.
  This quick select produces union of row sequences returned by several
  quick select it "merges".

  All merged quick selects must return rowids in rowid order.
  QuickRorUnionSelect will return rows in rowid order, too.

  All merged quick selects are set not to retrieve full table records.
  ROR-union quick select always retrieves full records.

*/
class QuickRorUnionSelect : public QuickSelectInterface
{
public:
  QuickRorUnionSelect(Session *session, Table *table);
  ~QuickRorUnionSelect();

  /**
   * Do post-constructor initialization.
   * SYNOPSIS
   * QuickRorUnionSelect::init()
   *
   * RETURN
   * @retval 0      OK
   * @retval other  Error code
   */
  int  init();

  /**
   * Initialize quick select for row retrieval.
   * SYNOPSIS
   * reset()
   *
   * RETURN
   * @retval 0      OK
   * @retval other  Error code
   */
  int  reset(void);

  /**
   * Retrieve next record.
   * SYNOPSIS
   * QuickRorUnionSelect::get_next()
   *
   * NOTES
   * Enter/exit invariant:
   * For each quick select in the queue a {key,rowid} tuple has been
   * retrieved but the corresponding row hasn't been passed to output.
   *
   * RETURN
   * @retval 0     - Ok
   * @retval other - Error code if any error occurred.
   */
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

  void add_keys_and_lengths(std::string *key_names, std::string *used_lengths);
  void add_info_string(std::string *str);
  bool is_keys_used(const boost::dynamic_bitset<>& fields);

  void push_quick_back(QuickSelectInterface *quick_sel_range);

  std::vector<QuickSelectInterface *> quick_selects; /**< Merged quick selects */

  /** Priority queue for merge operation */
  std::priority_queue<QuickSelectInterface *, std::vector<QuickSelectInterface *>, compare_functor > *queue;
  memory::Root alloc; /**< Memory pool for this and merged quick selects data. */

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

