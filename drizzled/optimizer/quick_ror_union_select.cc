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
#include <drizzled/util/functors.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/optimizer/quick_range_select.h>
#include <drizzled/optimizer/quick_ror_union_select.h>
#include <drizzled/table.h>
#include <drizzled/system_variables.h>

#include <vector>
#include <algorithm>

using namespace std;

namespace drizzled {

optimizer::QuickRorUnionSelect::QuickRorUnionSelect(Session *session_param,
                                                    Table *table)
  :
    session(session_param),
    scans_inited(false)
{
  index= MAX_KEY;
  head= table;
  rowid_length= table->cursor->ref_length;
  record= head->record[0];
  alloc.init(session->variables.range_alloc_block_size);
  session_param->mem_root= &alloc;
}

/*
 * Function object that is used as the comparison function
 * for the priority queue in the QuickRorUnionSelect
 * class.
 */
class optimizer::compare_functor
{
  optimizer::QuickRorUnionSelect *self;
  public:
  compare_functor(optimizer::QuickRorUnionSelect *in_arg)
    : self(in_arg) { }
  inline bool operator()(const optimizer::QuickSelectInterface *i, const optimizer::QuickSelectInterface *j) const
  {
    int val= self->head->cursor->cmp_ref(i->last_rowid,
                                         j->last_rowid);
    return (val >= 0);
  }
};


int optimizer::QuickRorUnionSelect::init()
{
  queue= new priority_queue<QuickSelectInterface*, vector<QuickSelectInterface*>, compare_functor>(compare_functor(this));
  cur_rowid= alloc.alloc(2 * head->cursor->ref_length);
  prev_rowid= cur_rowid + head->cursor->ref_length;
  return 0;
}


int optimizer::QuickRorUnionSelect::reset()
{
  have_prev_rowid= false;
  if (! scans_inited)
  {
    BOOST_FOREACH(QuickSelectInterface* it, quick_selects)
    {
      if (it->init_ror_merged_scan(false))
      {
        return 0;
      }
    }
    scans_inited= true;
  }
  while (! queue->empty())
  {
    queue->pop();
  }
  /*
    Initialize scans for merged quick selects and put all merged quick
    selects into the queue.
  */
  BOOST_FOREACH(QuickSelectInterface* it, quick_selects)
  {
    if (it->reset())
      return 0;
    if (it->get_next() == HA_ERR_END_OF_FILE)
      continue;
    it->save_last_pos();
    queue->push(it);
  }
  if (head->cursor->startTableScan(1))
    return 0;
  return 0;
}


void optimizer::QuickRorUnionSelect::push_quick_back(QuickSelectInterface *quick_sel_range)
{
  quick_selects.push_back(quick_sel_range);
}


optimizer::QuickRorUnionSelect::~QuickRorUnionSelect()
{
  while (! queue->empty())
  {
    queue->pop();
  }
  delete queue;
  for_each(quick_selects.begin(),
           quick_selects.end(),
           DeletePtr());
  quick_selects.clear();
  if (head->cursor->inited != Cursor::NONE)
  {
    head->cursor->endTableScan();
  }
  alloc.free_root(MYF(0));
}


bool optimizer::QuickRorUnionSelect::is_keys_used(const boost::dynamic_bitset<>& fields)
{
  BOOST_FOREACH(QuickSelectInterface* it, quick_selects)
  {
    if (it->is_keys_used(fields))
      return true;
  }
  return false;
}


int optimizer::QuickRorUnionSelect::get_next()
{
  int error;
  int dup_row;
  optimizer::QuickSelectInterface *quick= NULL;
  unsigned char *tmp= NULL;

  do
  {
    do
    {
      if (queue->empty())
        return HA_ERR_END_OF_FILE;
      /* Ok, we have a queue with >= 1 scans */

      quick= queue->top();
      memcpy(cur_rowid, quick->last_rowid, rowid_length);

      /* put into queue rowid from the same stream as top element */
      if ((error= quick->get_next()))
      {
        if (error != HA_ERR_END_OF_FILE)
          return(error);
        queue->pop();
      }
      else
      {
        quick->save_last_pos();
        queue->pop();
        queue->push(quick);
      }

      if (!have_prev_rowid)
      {
        /* No rows have been returned yet */
        dup_row= false;
        have_prev_rowid= true;
      }
      else
        dup_row= ! head->cursor->cmp_ref(cur_rowid, prev_rowid);
    } while (dup_row);

    tmp= cur_rowid;
    cur_rowid= prev_rowid;
    prev_rowid= tmp;

    error= head->cursor->rnd_pos(quick->record, prev_rowid);
  } while (error == HA_ERR_RECORD_DELETED);
  return error;
}


void optimizer::QuickRorUnionSelect::add_info_string(string *str)
{
  bool first= true;
  str->append("union(");
  BOOST_FOREACH(QuickSelectInterface* it, quick_selects)
  {
    if (first)
      first= false;
    else
      str->append(",");
    it->add_info_string(str);
  }
  str->append(")");
}


void optimizer::QuickRorUnionSelect::add_keys_and_lengths(string *key_names, string *used_lengths)
{
  bool first= true;
  BOOST_FOREACH(QuickSelectInterface* it, quick_selects)
  {
    if (first)
      first= false;
    else
    {
      used_lengths->append(",");
      key_names->append(",");
    }
    it->add_keys_and_lengths(key_names, used_lengths);
  }
}

} /* namespace drizzled */
