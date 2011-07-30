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
#include <drizzled/optimizer/quick_ror_intersect_select.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/key.h>
#include <drizzled/table.h>
#include <drizzled/system_variables.h>

#include <vector>

using namespace std;

namespace drizzled {

optimizer::QuickRorIntersectSelect::QuickRorIntersectSelect(Session *session_param,
                                                            Table *table,
                                                            bool retrieve_full_rows,
                                                            memory::Root *parent_alloc)
  :
    cpk_quick(NULL),
    session(session_param),
    need_to_fetch_row(retrieve_full_rows),
    scans_inited(false)
{
  index= MAX_KEY;
  head= table;
  record= head->record[0];
  if (! parent_alloc)
  {
    alloc.init(session->variables.range_alloc_block_size);
  }
  else
  {
    memset(&alloc, 0, sizeof(memory::Root));
  }

  last_rowid= parent_alloc
    ? parent_alloc->alloc(head->cursor->ref_length)
    : alloc.alloc(head->cursor->ref_length);
}


optimizer::QuickRorIntersectSelect::~QuickRorIntersectSelect()
{
  for_each(quick_selects.begin(),
           quick_selects.end(),
           DeletePtr());
  quick_selects.clear();
  delete cpk_quick;
  alloc.free_root(MYF(0));
  if (need_to_fetch_row && head->cursor->inited != Cursor::NONE)
  {
    head->cursor->endTableScan();
  }
  return;
}


int optimizer::QuickRorIntersectSelect::init()
{
 /* Check if last_rowid was successfully allocated in ctor */
  return (! last_rowid);
}


int optimizer::QuickRorIntersectSelect::init_ror_merged_scan(bool reuse_handler)
{
  vector<optimizer::QuickRangeSelect *>::iterator it= quick_selects.begin();

  /* Initialize all merged "children" quick selects */
  assert(! need_to_fetch_row || reuse_handler);
  if (! need_to_fetch_row && reuse_handler)
  {
    optimizer::QuickRangeSelect *quick= *it;
    ++it;
    /*
      There is no use of this->cursor. Use it for the first of merged range
      selects.
    */
    if (quick->init_ror_merged_scan(true))
      return 0;
    quick->cursor->extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);
  }
  while (it != quick_selects.end())
  {
    if ((*it)->init_ror_merged_scan(false))
    {
      return 0;
    }
    (*it)->cursor->extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);
    /* All merged scans share the same record buffer in intersection. */
    (*it)->record= head->record[0];
    ++it;
  }

  if (need_to_fetch_row && head->cursor->startTableScan(1))
  {
    return 0;
  }
  return 0;
}


int optimizer::QuickRorIntersectSelect::reset()
{
  if (! scans_inited && init_ror_merged_scan(true))
    return 0;
  scans_inited= true;
  BOOST_FOREACH(QuickRangeSelect* it, quick_selects)
    it->reset();
  return 0;
}


void optimizer::QuickRorIntersectSelect::push_quick_back(optimizer::QuickRangeSelect *quick)
{
  quick_selects.push_back(quick);
}


bool optimizer::QuickRorIntersectSelect::is_keys_used(const boost::dynamic_bitset<>& fields)
{
  BOOST_FOREACH(QuickRangeSelect* it, quick_selects)
  {
    if (is_key_used(head, it->index, fields))
      return 1;
  }
  return 0;
}


int optimizer::QuickRorIntersectSelect::get_next()
{
  optimizer::QuickRangeSelect *quick= NULL;
  vector<optimizer::QuickRangeSelect *>::iterator it= quick_selects.begin();
  int error;
  int cmp;
  uint32_t last_rowid_count= 0;

  do
  {
    /* Get a rowid for first quick and save it as a 'candidate' */
    quick= *it;
    ++it;
    error= quick->get_next();
    if (cpk_quick)
    {
      while (! error && ! cpk_quick->row_in_ranges())
        error= quick->get_next();
    }
    if (error)
      return error;

    quick->cursor->position(quick->record);
    memcpy(last_rowid, quick->cursor->ref, head->cursor->ref_length);
    last_rowid_count= 1;

    while (last_rowid_count < quick_selects.size())
    {
      /** @todo: fix this madness!!!! */
      if (it != quick_selects.end())
      {
        quick= *it;
        ++it;
      }
      else
      {
        it= quick_selects.begin();
        quick= *it;
        ++it;
      }

      do
      {
        if ((error= quick->get_next()))
          return error;
        quick->cursor->position(quick->record);
        cmp= head->cursor->cmp_ref(quick->cursor->ref, last_rowid);
      } while (cmp < 0);

      /* Ok, current select 'caught up' and returned ref >= cur_ref */
      if (cmp > 0)
      {
        /* Found a row with ref > cur_ref. Make it a new 'candidate' */
        if (cpk_quick)
        {
          while (! cpk_quick->row_in_ranges())
          {
            if ((error= quick->get_next()))
              return error;
          }
        }
        memcpy(last_rowid, quick->cursor->ref, head->cursor->ref_length);
        last_rowid_count= 1;
      }
      else
      {
        /* current 'candidate' row confirmed by this select */
        last_rowid_count++;
      }
    }

    /* We get here if we got the same row ref in all scans. */
    if (need_to_fetch_row)
      error= head->cursor->rnd_pos(head->record[0], last_rowid);
  } while (error == HA_ERR_RECORD_DELETED);
  return error;
}


void optimizer::QuickRorIntersectSelect::add_info_string(string *str)
{
  bool first= true;
  str->append("intersect(");
  BOOST_FOREACH(QuickRangeSelect* it, quick_selects)
  {
    KeyInfo *key_info= head->key_info + it->index;
    if (! first)
      str->append(",");
    else
      first= false;
    str->append(key_info->name);
  }
  if (cpk_quick)
  {
    KeyInfo *key_info= head->key_info + cpk_quick->index;
    str->append(",");
    str->append(key_info->name);
  }
  str->append(")");
}


void optimizer::QuickRorIntersectSelect::add_keys_and_lengths(string *key_names,
                                                              string *used_lengths)
{
  char buf[64];
  uint32_t length;
  bool first= true;
  BOOST_FOREACH(QuickRangeSelect* it, quick_selects)
  {
    KeyInfo *key_info= head->key_info + it->index;
    if (first)
    {
      first= false;
    }
    else
    {
      key_names->append(",");
      used_lengths->append(",");
    }
    key_names->append(key_info->name);
    length= internal::int64_t2str(it->max_used_key_length, buf, 10) - buf;
    used_lengths->append(buf, length);
  }

  if (cpk_quick)
  {
    KeyInfo *key_info= head->key_info + cpk_quick->index;
    key_names->append(",");
    key_names->append(key_info->name);
    length= internal::int64_t2str(cpk_quick->max_used_key_length, buf, 10) - buf;
    used_lengths->append(",");
    used_lengths->append(buf, length);
  }
}

} /* namespace drizzled */
