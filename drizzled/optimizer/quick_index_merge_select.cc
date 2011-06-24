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
#include <drizzled/records.h>
#include <drizzled/util/functors.h>
#include <drizzled/optimizer/quick_range_select.h>
#include <drizzled/optimizer/quick_index_merge_select.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/unique.h>
#include <drizzled/key.h>
#include <drizzled/table.h>
#include <drizzled/system_variables.h>

#include <vector>

using namespace std;

namespace drizzled {

static int refpos_order_cmp(void *arg, const void *a, const void *b)
{
  Cursor *cursor= (Cursor*)arg;
  return cursor->cmp_ref((const unsigned char *) a, (const unsigned char *) b);
}

optimizer::QuickIndexMergeSelect::QuickIndexMergeSelect(Session *session_param,
                                                        Table *table)
  :
    pk_quick_select(NULL),
    session(session_param)
{
  index= MAX_KEY;
  head= table;
  memset(&read_record, 0, sizeof(read_record));
  alloc.init(session->variables.range_alloc_block_size);
}

int optimizer::QuickIndexMergeSelect::init()
{
  return 0;
}

int optimizer::QuickIndexMergeSelect::reset()
{
  return (read_keys_and_merge());
}

void optimizer::QuickIndexMergeSelect::push_quick_back(optimizer::QuickRangeSelect *quick_sel_range)
{
  /*
    Save quick_select that does scan on clustered primary key as it will be
    processed separately.
  */
  if (head->cursor->primary_key_is_clustered() &&
      quick_sel_range->index == head->getShare()->getPrimaryKey())
  {
    pk_quick_select= quick_sel_range;
  }
  else
  {
    quick_selects.push_back(quick_sel_range);
  }
}

optimizer::QuickIndexMergeSelect::~QuickIndexMergeSelect()
{
  BOOST_FOREACH(QuickRangeSelect* it, quick_selects)
    it->cursor= NULL;
  for_each(quick_selects.begin(), quick_selects.end(), DeletePtr());
  quick_selects.clear();
  delete pk_quick_select;
  alloc.free_root(MYF(0));
}


int optimizer::QuickIndexMergeSelect::read_keys_and_merge()
{
  vector<optimizer::QuickRangeSelect *>::iterator it= quick_selects.begin();
  optimizer::QuickRangeSelect *cur_quick= NULL;
  int result;
  Unique *unique= NULL;
  Cursor *cursor= head->cursor;

  cursor->extra(HA_EXTRA_KEYREAD);
  head->prepare_for_position();

  cur_quick= *it;
  ++it;
  assert(cur_quick != 0);

  /*
    We reuse the same instance of Cursor so we need to call both init and
    reset here.
  */
  if (cur_quick->init() || cur_quick->reset())
    return 0;

  unique= new Unique(refpos_order_cmp,
                     (void *) cursor,
                     cursor->ref_length,
                     session->variables.sortbuff_size);
  if (! unique)
    return 0;
  for (;;)
  {
    while ((result= cur_quick->get_next()) == HA_ERR_END_OF_FILE)
    {
      cur_quick->range_end();
      if (it == quick_selects.end())
      {
        break;
      }
      cur_quick= *it;
      ++it;
      if (! cur_quick)
        break;

      if (cur_quick->cursor->inited != Cursor::NONE)
        cur_quick->cursor->endIndexScan();
      if (cur_quick->init() || cur_quick->reset())
        return 0;
    }

    if (result)
    {
      if (result != HA_ERR_END_OF_FILE)
      {
        cur_quick->range_end();
        return result;
      }
      break;
    }

    if (session->getKilled())
      return 0;

    /* skip row if it will be retrieved by clustered PK scan */
    if (pk_quick_select && pk_quick_select->row_in_ranges())
      continue;

    cur_quick->cursor->position(cur_quick->record);
    result= unique->unique_add((char*) cur_quick->cursor->ref);
    if (result)
      return 0;

  }

  /* ok, all row ids are in Unique */
  result= unique->get(head);
  delete unique;
  doing_pk_scan= false;
  /* index_merge currently doesn't support "using index" at all */
  cursor->extra(HA_EXTRA_NO_KEYREAD);
  /* start table scan */
  if ((result= read_record.init_read_record(session, head, (optimizer::SqlSelect*) 0, 1, 1)))
  {
    head->print_error(result, MYF(0));
    return 0;
  }
  return result;
}


int optimizer::QuickIndexMergeSelect::get_next()
{
  int result;

  if (doing_pk_scan)
    return(pk_quick_select->get_next());

  if ((result= read_record.read_record(&read_record)) == -1)
  {
    result= HA_ERR_END_OF_FILE;
    read_record.end_read_record();
    /* All rows from Unique have been retrieved, do a clustered PK scan */
    if (pk_quick_select)
    {
      doing_pk_scan= true;
      if ((result= pk_quick_select->init()) ||
          (result= pk_quick_select->reset()))
        return result;
      return(pk_quick_select->get_next());
    }
  }

  return result;
}

bool optimizer::QuickIndexMergeSelect::is_keys_used(const boost::dynamic_bitset<>& fields)
{
  BOOST_FOREACH(QuickRangeSelect* it, quick_selects)
  {
    if (is_key_used(head, it->index, fields))
      return 1;
  }
  return 0;
}


void optimizer::QuickIndexMergeSelect::add_info_string(string *str)
{
  bool first= true;
  str->append("sort_union(");
  BOOST_FOREACH(QuickRangeSelect* it, quick_selects)
  {
    if (! first)
      str->append(",");
    else
      first= false;
    it->add_info_string(str);
  }
  if (pk_quick_select)
  {
    str->append(",");
    pk_quick_select->add_info_string(str);
  }
  str->append(")");
}


void optimizer::QuickIndexMergeSelect::add_keys_and_lengths(string *key_names,
                                                            string *used_lengths)
{
  char buf[64];
  uint32_t length= 0;
  bool first= true;

  BOOST_FOREACH(QuickRangeSelect* it, quick_selects)
  {
    if (first)
      first= false;
    else
    {
      key_names->append(",");
      used_lengths->append(",");
    }

    KeyInfo *key_info= head->key_info + it->index;
    key_names->append(key_info->name);
    length= internal::int64_t2str(it->max_used_key_length, buf, 10) - buf;
    used_lengths->append(buf, length);
  }
  if (pk_quick_select)
  {
    KeyInfo *key_info= head->key_info + pk_quick_select->index;
    key_names->append(",");
    key_names->append(key_info->name);
    length= internal::int64_t2str(pk_quick_select->max_used_key_length, buf, 10) - buf;
    used_lengths->append(",");
    used_lengths->append(buf, length);
  }
}


} /* namespace drizzled */
