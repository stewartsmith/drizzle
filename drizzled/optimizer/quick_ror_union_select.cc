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

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/optimizer/range.h"
#include "drizzled/optimizer/quick_range_select.h"
#include "drizzled/optimizer/quick_ror_union_select.h"

#include <algorithm>

using namespace std;
using namespace drizzled;


optimizer::QUICK_ROR_UNION_SELECT::QUICK_ROR_UNION_SELECT(Session *session_param,
                                                          Table *table)
  :
    session(session_param),
    scans_inited(false)
{
  index= MAX_KEY;
  head= table;
  rowid_length= table->cursor->ref_length;
  record= head->record[0];
  init_sql_alloc(&alloc, session->variables.range_alloc_block_size, 0);
  session_param->mem_root= &alloc;
}

/*
 * Function object that is used as the comparison function
 * for the priority queue in the QUICK_ROR_UNION_SELECT
 * class.
 */
class optimizer::compare_functor
{
  optimizer::QUICK_ROR_UNION_SELECT *self;
  public:
  compare_functor(optimizer::QUICK_ROR_UNION_SELECT *in_arg)
    : self(in_arg) { }
  inline bool operator()(const optimizer::QuickSelectInterface *i, const optimizer::QuickSelectInterface *j) const
  {
    int val= self->head->cursor->cmp_ref(i->last_rowid,
                                         j->last_rowid);
    return (val >= 0);
  }
};

/*
  Do post-constructor initialization.
  SYNOPSIS
    QUICK_ROR_UNION_SELECT::init()

  RETURN
    0      OK
    other  Error code
*/
int optimizer::QUICK_ROR_UNION_SELECT::init()
{
  queue=
    new priority_queue<optimizer::QuickSelectInterface *,
                       vector<optimizer::QuickSelectInterface *>,
                       optimizer::compare_functor >(optimizer::compare_functor(this));
  if (! (cur_rowid= (unsigned char*) alloc_root(&alloc, 2*head->cursor->ref_length)))
  {
    return 0;
  }
  prev_rowid= cur_rowid + head->cursor->ref_length;
  return 0;
}


/*
  Initialize quick select for row retrieval.
  SYNOPSIS
    reset()

  RETURN
    0      OK
    other  Error code
*/
int optimizer::QUICK_ROR_UNION_SELECT::reset()
{
  QuickSelectInterface *quick= NULL;
  int error;
  have_prev_rowid= false;
  if (! scans_inited)
  {
    List_iterator_fast<QuickSelectInterface> it(quick_selects);
    while ((quick= it++))
    {
      if (quick->init_ror_merged_scan(false))
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
  List_iterator_fast<QuickSelectInterface> it(quick_selects);
  while ((quick= it++))
  {
    if (quick->reset())
    {
      return 0;
    }
    if ((error= quick->get_next()))
    {
      if (error == HA_ERR_END_OF_FILE)
      {
        continue;
      }
      return(error);
    }
    quick->save_last_pos();
    queue->push(quick);
  }

  if (head->cursor->ha_rnd_init(1))
  {
    return 0;
  }

  return 0;
}


bool
optimizer::QUICK_ROR_UNION_SELECT::push_quick_back(QuickSelectInterface *quick_sel_range)
{
  return quick_selects.push_back(quick_sel_range);
}


optimizer::QUICK_ROR_UNION_SELECT::~QUICK_ROR_UNION_SELECT()
{
  while (! queue->empty())
  {
    queue->pop();
  }
  delete queue;
  quick_selects.delete_elements();
  if (head->cursor->inited != Cursor::NONE)
  {
    head->cursor->ha_rnd_end();
  }
  free_root(&alloc,MYF(0));
  return;
}


bool optimizer::QUICK_ROR_UNION_SELECT::is_keys_used(const MyBitmap *fields)
{
  optimizer::QuickSelectInterface *quick;
  List_iterator_fast<optimizer::QuickSelectInterface> it(quick_selects);
  while ((quick= it++))
  {
    if (quick->is_keys_used(fields))
      return 1;
  }
  return 0;
}


/*
  Retrieve next record.
  SYNOPSIS
    QUICK_ROR_UNION_SELECT::get_next()

  NOTES
    Enter/exit invariant:
    For each quick select in the queue a {key,rowid} tuple has been
    retrieved but the corresponding row hasn't been passed to output.

  RETURN
   0     - Ok
   other - Error code if any error occurred.
*/
int optimizer::QUICK_ROR_UNION_SELECT::get_next()
{
  int error, dup_row;
  optimizer::QuickSelectInterface *quick;
  unsigned char *tmp;

  do
  {
    do
    {
      if (queue->empty())
        return(HA_ERR_END_OF_FILE);
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
        dup_row= !head->cursor->cmp_ref(cur_rowid, prev_rowid);
    } while (dup_row);

    tmp= cur_rowid;
    cur_rowid= prev_rowid;
    prev_rowid= tmp;

    error= head->cursor->rnd_pos(quick->record, prev_rowid);
  } while (error == HA_ERR_RECORD_DELETED);
  return(error);
}


void optimizer::QUICK_ROR_UNION_SELECT::add_info_string(String *str)
{
  bool first= true;
  optimizer::QuickSelectInterface *quick;
  List_iterator_fast<optimizer::QuickSelectInterface> it(quick_selects);
  str->append(STRING_WITH_LEN("union("));
  while ((quick= it++))
  {
    if (! first)
      str->append(',');
    else
      first= false;
    quick->add_info_string(str);
  }
  str->append(')');
}


void optimizer::QUICK_ROR_UNION_SELECT::add_keys_and_lengths(String *key_names,
                                                             String *used_lengths)
{
  bool first= true;
  optimizer::QuickSelectInterface *quick;
  List_iterator_fast<optimizer::QuickSelectInterface> it(quick_selects);
  while ((quick= it++))
  {
    if (first)
      first= false;
    else
    {
      used_lengths->append(',');
      key_names->append(',');
    }
    quick->add_keys_and_lengths(key_names, used_lengths);
  }
}

