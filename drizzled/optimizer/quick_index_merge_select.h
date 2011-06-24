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
#include <drizzled/records.h>

#include <boost/dynamic_bitset.hpp>
#include <vector>

namespace drizzled
{

namespace optimizer
{

/**
  @class QuickIndexMergeSelect - index_merge access method quick select.

    QuickIndexMergeSelect uses
     * QuickRangeSelects to get rows
     * Unique class to remove duplicate rows

  INDEX MERGE OPTIMIZER
    Current implementation doesn't detect all cases where index_merge could
    be used, in particular:
     * index_merge will never be used if range scan is possible (even if
       range scan is more expensive)

     * index_merge+'using index' is not supported (this the consequence of
       the above restriction)

     * If WHERE part contains complex nested AND and OR conditions, some ways
       to retrieve rows using index_merge will not be considered. The choice
       of read plan may depend on the order of conjuncts/disjuncts in WHERE
       part of the query, see comments near imerge_list_or_list and
       SEL_IMERGE::or_sel_tree_with_checks functions for details.

     * There is no "index_merge_ref" method (but index_merge on non-first
       table in join is possible with 'range checked for each record').

    See comments around SEL_IMERGE class and test_quick_select for more
    details.

  ROW RETRIEVAL ALGORITHM

    index_merge uses Unique class for duplicates removal.  index_merge takes
    advantage of Clustered Primary Key (CPK) if the table has one.
    The index_merge algorithm consists of two phases:

    Phase 1 (implemented in QuickIndexMergeSelect::prepare_unique):
    prepare()
    {
      activate 'index only';
      while(retrieve next row for non-CPK scan)
      {
        if (there is a CPK scan and row will be retrieved by it)
          skip this row;
        else
          put its rowid into Unique;
      }
      deactivate 'index only';
    }

    Phase 2 (implemented as sequence of QuickIndexMergeSelect::get_next
    calls):

    fetch()
    {
      retrieve all rows from row pointers stored in Unique;
      free Unique;
      retrieve all rows for CPK scan;
    }
*/
class QuickIndexMergeSelect : public QuickSelectInterface
{
public:

  QuickIndexMergeSelect(Session *session, Table *table);

  ~QuickIndexMergeSelect();

  int init();
  int reset(void);

  /**
   * Get next row for index_merge.
   * NOTES
   * The rows are read from
   * 1. rowids stored in Unique.
   * 2. QuickRangeSelect with clustered primary key (if any).
   * The sets of rows retrieved in 1) and 2) are guaranteed to be disjoint.
   */
  int get_next();

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
    return QS_TYPE_INDEX_MERGE;
  }

  void add_keys_and_lengths(std::string *key_names, std::string *used_lengths);
  void add_info_string(std::string *str);
  bool is_keys_used(const boost::dynamic_bitset<>& fields);

  void push_quick_back(QuickRangeSelect *quick_sel_range);

  /* range quick selects this index_merge read consists of */
  std::vector<QuickRangeSelect *> quick_selects;

  /* quick select that uses clustered primary key (NULL if none) */
  QuickRangeSelect *pk_quick_select;

  /* true if this select is currently doing a clustered PK scan */
  bool  doing_pk_scan;

  memory::Root alloc;
  Session *session;

  /**
   * Perform key scans for all used indexes (except CPK), get rowids and merge
   * them into an ordered non-recurrent sequence of rowids.
   *
   * The merge/duplicate removal is performed using Unique class. We put all
   * rowids into Unique, get the sorted sequence and destroy the Unique.
   *
   * If table has a clustered primary key that covers all rows (true for bdb
   * and innodb currently) and one of the index_merge scans is a scan on PK,
   * then rows that will be retrieved by PK scan are not put into Unique and
   * primary key scan is not performed here, it is performed later separately.
   *
   * RETURN
   * @retval 0     OK
   * @retval other error
   */
  int read_keys_and_merge();

  /* used to get rows collected in Unique */
  ReadRecord read_record;
};

} /* namespace optimizer */

} /* namespace drizzled */

