/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Padraig O'Sullivan
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include <drizzled/join_table.h>
#include <drizzled/table.h>
#include <drizzled/sql_select.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/optimizer/access_method/scan.h>
#include <drizzled/util/test.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/session.h>

using namespace drizzled;

static uint32_t make_join_orderinfo(Join *join);

void optimizer::Scan::getStats(Table& table, JoinTable& join_tab)
{
  Join *join= join_tab.join;
  bool statistics= test(! (join->select_options & SELECT_DESCRIBE));
  uint64_t options= (join->select_options &
                     (SELECT_DESCRIBE | SELECT_NO_JOIN_CACHE)) |
                     (0);
  uint32_t no_jbuf_after= make_join_orderinfo(join);
  uint32_t index= &join_tab - join->join_tab;

  /*
   * If previous table use cache
   * If the incoming data set is already sorted don't use cache.
   */
  table.status= STATUS_NO_RECORD;

  if (index != join->const_tables && 
      ! (options & SELECT_NO_JOIN_CACHE) &&
      join_tab.use_quick != 2 && 
      ! join_tab.first_inner && 
      index <= no_jbuf_after &&
      ! join_tab.insideout_match_tab)
  {
    if ((options & SELECT_DESCRIBE) ||
        ! join_init_cache(join->session,
                          join->join_tab + join->const_tables,
                          index - join->const_tables))
    {
      (&join_tab)[-1].next_select= sub_select_cache; /* Patch previous */
    }
  }

  /* These init changes read_record */
  if (join_tab.use_quick == 2)
  {
    join->session->server_status|= SERVER_QUERY_NO_GOOD_INDEX_USED;
    join_tab.read_first_record= join_init_quick_read_record;
    if (statistics)
    {
      join->session->status_var.select_range_check_count++;
    }
  }
  else
  {
    join_tab.read_first_record= join_init_read_record;
    if (index == join->const_tables)
    {
      if (join_tab.select && join_tab.select->quick)
      {
        if (statistics)
          join->session->status_var.select_range_count++;
      }
      else
      {
        join->session->server_status|= SERVER_QUERY_NO_INDEX_USED;
        if (statistics)
          join->session->status_var.select_scan_count++;
      }
    }
    else
    {
      if (join_tab.select && join_tab.select->quick)
      {
        if (statistics)
          join->session->status_var.select_full_range_join_count++;
      }
      else
      {
        join->session->server_status|= SERVER_QUERY_NO_INDEX_USED;
        if (statistics)
          join->session->status_var.select_full_join_count++;
      }
    }
    if (! table.no_keyread)
    {
      if (join_tab.select && 
          join_tab.select->quick &&
          join_tab.select->quick->index != MAX_KEY && //not index_merge
          table.covering_keys.test(join_tab.select->quick->index))
      {
        table.key_read= 1;
        table.cursor->extra(HA_EXTRA_KEYREAD);
      }
      else if (! table.covering_keys.none() && ! (join_tab.select && join_tab.select->quick))
      { // Only read index tree
        if (! join_tab.insideout_match_tab)
        {
          /*
             See bug #26447: "Using the clustered index for a table scan
             is always faster than using a secondary index".
           */
          if (table.getShare()->hasPrimaryKey() &&
              table.cursor->primary_key_is_clustered())
          {
            join_tab.index= table.getShare()->getPrimaryKey();
          }
          else
          {
            join_tab.index= table.find_shortest_key(&table.covering_keys);
          }
        }
        join_tab.read_first_record= join_read_first;
        join_tab.type= AM_NEXT; // Read with index_first / index_next
      }
    }
  }
}

/**
  Determine if the set is already ordered for order_st BY, so it can
  disable join cache because it will change the ordering of the results.
  Code handles sort table that is at any location (not only first after
  the const tables) despite the fact that it's currently prohibited.
  We must disable join cache if the first non-const table alone is
  ordered. If there is a temp table the ordering is done as a last
  operation and doesn't prevent join cache usage.
*/
static uint32_t make_join_orderinfo(Join *join)
{
  if (join->need_tmp)
    return join->tables;

  uint32_t i= join->const_tables;
  for (; i < join->tables; i++)
  {
    JoinTable *tab= join->join_tab + i;
    Table *table= tab->table;
    if ((table == join->sort_by_table &&
        (! join->order || join->skip_sort_order)) ||
        (join->sort_by_table == (Table *) 1 &&  i != join->const_tables))
    {
      break;
    }
  }
  return i;
}
