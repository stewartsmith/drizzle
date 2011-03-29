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

#include <config.h>

#include <drizzled/session.h>
#include <drizzled/item/uint.h>
#include <drizzled/item/float.h>
#include <drizzled/item/string.h>
#include <drizzled/optimizer/explain_plan.h>
#include <drizzled/optimizer/position.h>
#include <drizzled/optimizer/quick_ror_intersect_select.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/sql_select.h>
#include <drizzled/join.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/select_result.h>
#include <drizzled/sql_lex.h>

#include <cstdio>
#include <string>
#include <sstream>
#include <bitset>

using namespace std;

namespace drizzled
{

static const string access_method_str[]=
{
  "UNKNOWN",
  "system",
  "const",
  "eq_ref",
  "ref",
  "MAYBE_REF",
  "ALL",
  "range",
  "index",
  "ref_or_null",
  "unique_subquery",
  "index_subquery",
  "index_merge"
};

static const string select_type_str[]=
{
  "PRIMARY",
  "SIMPLE",
  "DERIVED",
  "DEPENDENT SUBQUERY",
  "UNCACHEABLE SUBQUERY",
  "SUBQUERY",
  "DEPENDENT UNION",
  "UNCACHEABLE_UNION",
  "UNION",
  "UNION RESULT"
};

void optimizer::ExplainPlan::printPlan()
{
  List<Item> item_list;
  Session *session= join->session;
  select_result *result= join->result;
  Item *item_null= new Item_null();
  const charset_info_st * const cs= system_charset_info;
  int quick_type;
  /* Don't log this into the slow query log */
  session->server_status&= ~(SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED);
  join->unit->offset_limit_cnt= 0;

  /*
   NOTE: the number/types of items pushed into item_list must be in sync with
   EXPLAIN column types as they're "defined" in Session::send_explain_fields()
   */
  if (message)
  {
    item_list.push_back(new Item_int((int32_t)
                        join->select_lex->select_number));
    item_list.push_back(new Item_string(select_type_str[join->select_lex->type].c_str(),
                                        select_type_str[join->select_lex->type].length(),
                                        cs));
    for (uint32_t i= 0; i < 7; i++)
      item_list.push_back(item_null);

    if (join->session->lex().describe & DESCRIBE_EXTENDED)
      item_list.push_back(item_null);

    item_list.push_back(new Item_string(message,strlen(message),cs));
    if (result->send_data(item_list))
      join->error= 1;
  }
  else if (join->select_lex == join->unit->fake_select_lex)
  {
    /*
       here we assume that the query will return at least two rows, so we
       show "filesort" in EXPLAIN. Of course, sometimes we'll be wrong
       and no filesort will be actually done, but executing all selects in
       the UNION to provide precise EXPLAIN information will hardly be
       appreciated :)
     */
    char table_name_buffer[NAME_LEN];
    item_list.clear();
    /* id */
    item_list.push_back(new Item_null);
    /* select_type */
    item_list.push_back(new Item_string(select_type_str[join->select_lex->type].c_str(),
                                        select_type_str[join->select_lex->type].length(),
                                        cs));
    /* table */
    {
      Select_Lex *sl= join->unit->first_select();
      uint32_t len= 6, lastop= 0;
      memcpy(table_name_buffer, STRING_WITH_LEN("<union"));
      for (; sl && len + lastop + 5 < NAME_LEN; sl= sl->next_select())
      {
        len+= lastop;
        lastop= snprintf(table_name_buffer + len, NAME_LEN - len,
            "%u,", sl->select_number);
      }
      if (sl || len + lastop >= NAME_LEN)
      {
        memcpy(table_name_buffer + len, STRING_WITH_LEN("...>") + 1);
        len+= 4;
      }
      else
      {
        len+= lastop;
        table_name_buffer[len - 1]= '>';  // change ',' to '>'
      }
      item_list.push_back(new Item_string(table_name_buffer, len, cs));
    }
    /* type */
    item_list.push_back(new Item_string(access_method_str[AM_ALL].c_str(),
                                        access_method_str[AM_ALL].length(),
                                        cs));
    /* possible_keys */
    item_list.push_back(item_null);
    /* key*/
    item_list.push_back(item_null);
    /* key_len */
    item_list.push_back(item_null);
    /* ref */
    item_list.push_back(item_null);
    /* in_rows */
    if (join->session->lex().describe & DESCRIBE_EXTENDED)
      item_list.push_back(item_null);
    /* rows */
    item_list.push_back(item_null);
    /* extra */
    if (join->unit->global_parameters->order_list.first)
      item_list.push_back(new Item_string("Using filesort",
                                          14, 
                                          cs));
    else
      item_list.push_back(new Item_string("", 0, cs));

    if (result->send_data(item_list))
      join->error= 1;
  }
  else
  {
    table_map used_tables= 0;
    for (uint32_t i= 0; i < join->tables; i++)
    {
      JoinTable *tab= join->join_tab + i;
      Table *table= tab->table;
      char keylen_str_buf[64];
      string extra;
      char table_name_buffer[NAME_LEN];
      string tmp1;
      string tmp2;
      string tmp3;

      quick_type= -1;
      item_list.clear();
      /* id */
      item_list.push_back(new Item_uint((uint32_t)
            join->select_lex->select_number));
      /* select_type */
      item_list.push_back(new Item_string(select_type_str[join->select_lex->type].c_str(),
                                          select_type_str[join->select_lex->type].length(),
                                          cs));
      if (tab->type == AM_ALL && tab->select && tab->select->quick)
      {
        quick_type= tab->select->quick->get_type();
        if ((quick_type == optimizer::QuickSelectInterface::QS_TYPE_INDEX_MERGE) ||
            (quick_type == optimizer::QuickSelectInterface::QS_TYPE_ROR_INTERSECT) ||
            (quick_type == optimizer::QuickSelectInterface::QS_TYPE_ROR_UNION))
          tab->type = AM_INDEX_MERGE;
        else
          tab->type = AM_RANGE;
      }
      /* table */
      if (table->derived_select_number)
      {
        /* Derived table name generation */
        int len= snprintf(table_name_buffer, 
                          sizeof(table_name_buffer)-1,
                          "<derived%u>",
                          table->derived_select_number);
        item_list.push_back(new Item_string(table_name_buffer, len, cs));
      }
      else
      {
        TableList *real_table= table->pos_in_table_list;
        item_list.push_back(new Item_string(real_table->alias,
                                            strlen(real_table->alias),
                                            cs));
      }
      /* "type" column */
      item_list.push_back(new Item_string(access_method_str[tab->type].c_str(),
                                          access_method_str[tab->type].length(),
                                          cs));
      /* Build "possible_keys" value and add it to item_list */
      if (tab->keys.any())
      {
        for (uint32_t j= 0; j < table->getShare()->sizeKeys(); j++)
        {
          if (tab->keys.test(j))
          {
            if (tmp1.length())
              tmp1.append(",");
            tmp1.append(table->key_info[j].name,
                        strlen(table->key_info[j].name));
          }
        }
      }
      if (tmp1.length())
        item_list.push_back(new Item_string(tmp1.c_str(),tmp1.length(),cs));
      else
        item_list.push_back(item_null);

      /* Build "key", "key_len", and "ref" values and add them to item_list */
      if (tab->ref.key_parts)
      {
        KeyInfo *key_info= table->key_info+ tab->ref.key;
        item_list.push_back(new Item_string(key_info->name,
                                            strlen(key_info->name),
                                            system_charset_info));
        uint32_t length= internal::int64_t2str(tab->ref.key_length, keylen_str_buf, 10) -
                                     keylen_str_buf;
        item_list.push_back(new Item_string(keylen_str_buf, 
                                            length,
                                            system_charset_info));
        for (StoredKey **ref= tab->ref.key_copy; *ref; ref++)
        {
          if (tmp2.length())
            tmp2.append(",");
          tmp2.append((*ref)->name(),
                      strlen((*ref)->name()));
        }
        item_list.push_back(new Item_string(tmp2.c_str(),tmp2.length(),cs));
      }
      else if (tab->type == AM_NEXT)
      {
        KeyInfo *key_info=table->key_info+ tab->index;
        item_list.push_back(new Item_string(key_info->name,
              strlen(key_info->name),cs));
        uint32_t length= internal::int64_t2str(key_info->key_length, keylen_str_buf, 10) -
                                     keylen_str_buf;
        item_list.push_back(new Item_string(keylen_str_buf,
                                            length,
                                            system_charset_info));
        item_list.push_back(item_null);
      }
      else if (tab->select && tab->select->quick)
      {
        tab->select->quick->add_keys_and_lengths(&tmp2, &tmp3);
        item_list.push_back(new Item_string(tmp2.c_str(),tmp2.length(),cs));
        item_list.push_back(new Item_string(tmp3.c_str(),tmp3.length(),cs));
        item_list.push_back(item_null);
      }
      else
      {
        item_list.push_back(item_null);
        item_list.push_back(item_null);
        item_list.push_back(item_null);
      }

      /* Add "rows" field to item_list. */
      double examined_rows;
      if (tab->select && tab->select->quick)
      {
        examined_rows= tab->select->quick->records;
      }
      else if (tab->type == AM_NEXT || tab->type == AM_ALL)
      {
        examined_rows= tab->limit ? tab->limit : tab->table->cursor->records();
      }
      else
      {
        optimizer::Position cur_pos= join->getPosFromOptimalPlan(i);
        examined_rows= cur_pos.getFanout();
      }

      item_list.push_back(new Item_int((int64_t) (uint64_t) examined_rows,
                                       MY_INT64_NUM_DECIMAL_DIGITS));

      /* Add "filtered" field to item_list. */
      if (join->session->lex().describe & DESCRIBE_EXTENDED)
      {
        float f= 0.0;
        if (examined_rows)
        {
          optimizer::Position cur_pos= join->getPosFromOptimalPlan(i);
          f= static_cast<float>(100.0 * cur_pos.getFanout() / examined_rows);
        }
        item_list.push_back(new Item_float(f, 2));
      }

      /* Build "Extra" field and add it to item_list. */
      bool key_read= table->key_read;
      if ((tab->type == AM_NEXT || tab->type == AM_CONST) &&
          table->covering_keys.test(tab->index))
        key_read= 1;
      if (quick_type == optimizer::QuickSelectInterface::QS_TYPE_ROR_INTERSECT &&
          ! ((optimizer::QuickRorIntersectSelect *) tab->select->quick)->need_to_fetch_row)
        key_read= 1;

      if (tab->info)
        item_list.push_back(new Item_string(tab->info,strlen(tab->info),cs));
      else if (tab->packed_info & TAB_INFO_HAVE_VALUE)
      {
        if (tab->packed_info & TAB_INFO_USING_INDEX)
          extra.append("; Using index");
        if (tab->packed_info & TAB_INFO_USING_WHERE)
          extra.append("; Using where");
        if (tab->packed_info & TAB_INFO_FULL_SCAN_ON_NULL)
          extra.append("; Full scan on NULL key");
        if (extra.length())
          extra.erase(0, 2);        /* Skip initial "; "*/
        item_list.push_back(new Item_string(extra.c_str(), extra.length(), cs));
      }
      else
      {
        if (quick_type == optimizer::QuickSelectInterface::QS_TYPE_ROR_UNION ||
            quick_type == optimizer::QuickSelectInterface::QS_TYPE_ROR_INTERSECT ||
            quick_type == optimizer::QuickSelectInterface::QS_TYPE_INDEX_MERGE)
        {
          extra.append("; Using ");
          tab->select->quick->add_info_string(&extra);
        }
        if (tab->select)
        {
          if (tab->use_quick == 2)
          {
            /*
             * To print out the bitset in tab->keys, we go through
             * it 32 bits at a time. We need to do this to ensure
             * that the to_ulong() method will not throw an
             * out_of_range exception at runtime which would happen
             * if the bitset we were working with was larger than 64
             * bits on a 64-bit platform (for example).
             */
            stringstream s, w;
            string str;
            w << tab->keys;
            w >> str;
            for (uint32_t pos= 0; pos < tab->keys.size(); pos+= 32)
            {
              bitset<32> tmp(str, pos, 32);
              if (tmp.any())
                s << uppercase << hex << tmp.to_ulong();
            }
            extra.append("; Range checked for each record (index map: 0x");
            extra.append(s.str().c_str());
            extra.append(")");
          }
          else if (tab->select->cond)
          {
            extra.append("; Using where");
          }
        }
        if (key_read)
        {
          if (quick_type == optimizer::QuickSelectInterface::QS_TYPE_GROUP_MIN_MAX)
            extra.append("; Using index for group-by");
          else
            extra.append("; Using index");
        }
        if (table->reginfo.not_exists_optimize)
          extra.append("; Not exists");

        if (need_tmp_table)
        {
          need_tmp_table=0;
          extra.append("; Using temporary");
        }
        if (need_order)
        {
          need_order=0;
          extra.append("; Using filesort");
        }
        if (distinct & test_all_bits(used_tables,session->used_tables))
          extra.append("; Distinct");

        if (tab->insideout_match_tab)
        {
          extra.append("; LooseScan");
        }

        for (uint32_t part= 0; part < tab->ref.key_parts; part++)
        {
          if (tab->ref.cond_guards[part])
          {
            extra.append("; Full scan on NULL key");
            break;
          }
        }

        if (i > 0 && tab[-1].next_select == sub_select_cache)
          extra.append("; Using join buffer");

        if (extra.length())
          extra.erase(0, 2);
        item_list.push_back(new Item_string(extra.c_str(), extra.length(), cs));
      }
      // For next iteration
      used_tables|=table->map;
      if (result->send_data(item_list))
        join->error= 1;
    }
  }
  for (Select_Lex_Unit *unit= join->select_lex->first_inner_unit();
      unit;
      unit= unit->next_unit())
  {
    if (explainUnion(session, unit, result))
      return;
  }
  return;
}

bool optimizer::ExplainPlan::explainUnion(Session *session,
                                          Select_Lex_Unit *unit,
                                          select_result *result)
{
  bool res= false;
  Select_Lex *first= unit->first_select();

  for (Select_Lex *sl= first;
       sl;
       sl= sl->next_select())
  {
    // drop UNCACHEABLE_EXPLAIN, because it is for internal usage only
    sl->uncacheable.reset(UNCACHEABLE_EXPLAIN);
    if (&session->lex().select_lex == sl)
    {
      if (sl->first_inner_unit() || sl->next_select())
      {
        sl->type= optimizer::ST_PRIMARY;
      }
      else
      {
        sl->type= optimizer::ST_SIMPLE;
      }
    }
    else
    {
      if (sl == first)
      {
        if (sl->linkage == DERIVED_TABLE_TYPE)
        {
          sl->type= optimizer::ST_DERIVED;
        }
        else
        {
          if (sl->uncacheable.test(UNCACHEABLE_DEPENDENT))
          {
            sl->type= optimizer::ST_DEPENDENT_SUBQUERY;
          }
          else
          {
            if (sl->uncacheable.any())
            {
              sl->type= optimizer::ST_UNCACHEABLE_SUBQUERY;
            }
            else
            {
              sl->type= optimizer::ST_SUBQUERY;
            }
          }
        }
      }
      else
      {
        if (sl->uncacheable.test(UNCACHEABLE_DEPENDENT))
        {
          sl->type= optimizer::ST_DEPENDENT_UNION;
        }
        else
        {
          if (sl->uncacheable.any())
          {
            sl->type= optimizer::ST_UNCACHEABLE_UNION;
          }
          else
          {
            sl->type= optimizer::ST_UNION;
          }
        }
      }
    }
    sl->options|= SELECT_DESCRIBE;
  }

  if (unit->is_union())
  {
    unit->fake_select_lex->select_number= UINT_MAX; // just for initialization
    unit->fake_select_lex->type= optimizer::ST_UNION_RESULT;
    unit->fake_select_lex->options|= SELECT_DESCRIBE;
    if (! (res= unit->prepare(session, result, SELECT_NO_UNLOCK | SELECT_DESCRIBE)))
    {
      res= unit->exec();
    }
    res|= unit->cleanup();
  }
  else
  {
    session->lex().current_select= first;
    unit->set_limit(unit->global_parameters);
    res= select_query(session, 
                      &first->ref_pointer_array,
                      (TableList*) first->table_list.first,
                      first->with_wild, 
                      first->item_list,
                      first->where,
                      first->order_list.elements + first->group_list.elements,
                      (Order*) first->order_list.first,
                      (Order*) first->group_list.first,
                      first->having,
                      first->options | session->options | SELECT_DESCRIBE,
                      result, 
                      unit, 
                      first);
  }
  return (res || session->is_error());
}

} /* namespace drizzled */
