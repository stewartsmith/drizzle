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
#include "drizzled/optimizer/quick_ror_intersect_select.h"

using namespace drizzled;


optimizer::QUICK_ROR_INTERSECT_SELECT::QUICK_ROR_INTERSECT_SELECT(Session *session_param,
                                                                  Table *table,
                                                                  bool retrieve_full_rows,
                                                                  MEM_ROOT *parent_alloc)
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
    init_sql_alloc(&alloc, session->variables.range_alloc_block_size, 0);
  }
  else
  {
    memset(&alloc, 0, sizeof(MEM_ROOT));
  }
  last_rowid= (unsigned char*) alloc_root(parent_alloc ? parent_alloc : &alloc,
                                          head->cursor->ref_length);
}


optimizer::QUICK_ROR_INTERSECT_SELECT::~QUICK_ROR_INTERSECT_SELECT()
{
  quick_selects.delete_elements();
  delete cpk_quick;
  free_root(&alloc,MYF(0));
  if (need_to_fetch_row && head->cursor->inited != Cursor::NONE)
  {
    head->cursor->ha_rnd_end();
  }
  return;
}


int optimizer::QUICK_ROR_INTERSECT_SELECT::init()
{
 /* Check if last_rowid was successfully allocated in ctor */
  return (! last_rowid);
}


int optimizer::QUICK_ROR_INTERSECT_SELECT::init_ror_merged_scan(bool reuse_handler)
{
  List_iterator_fast<optimizer::QuickRangeSelect> quick_it(quick_selects);
  optimizer::QuickRangeSelect *quick= NULL;

  /* Initialize all merged "children" quick selects */
  assert(!need_to_fetch_row || reuse_handler);
  if (! need_to_fetch_row && reuse_handler)
  {
    quick= quick_it++;
    /*
      There is no use of this->cursor. Use it for the first of merged range
      selects.
    */
    if (quick->init_ror_merged_scan(true))
      return 0;
    quick->cursor->extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);
  }
  while ((quick= quick_it++))
  {
    if (quick->init_ror_merged_scan(false))
    {
      return 0;
    }
    quick->cursor->extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);
    /* All merged scans share the same record buffer in intersection. */
    quick->record= head->record[0];
  }

  if (need_to_fetch_row && head->cursor->ha_rnd_init(1))
  {
    return 0;
  }
  return 0;
}


int optimizer::QUICK_ROR_INTERSECT_SELECT::reset()
{
  if (! scans_inited && init_ror_merged_scan(true))
  {
    return 0;
  }
  scans_inited= true;
  List_iterator_fast<optimizer::QuickRangeSelect> it(quick_selects);
  optimizer::QuickRangeSelect *quick= NULL;
  while ((quick= it++))
  {
    quick->reset();
  }
  return 0;
}


bool
optimizer::QUICK_ROR_INTERSECT_SELECT::push_quick_back(optimizer::QuickRangeSelect *quick)
{
  return quick_selects.push_back(quick);
}


bool optimizer::QUICK_ROR_INTERSECT_SELECT::is_keys_used(const MyBitmap *fields)
{
  optimizer::QuickRangeSelect *quick;
  List_iterator_fast<optimizer::QuickRangeSelect> it(quick_selects);
  while ((quick= it++))
  {
    if (is_key_used(head, quick->index, fields))
      return 1;
  }
  return 0;
}


int optimizer::QUICK_ROR_INTERSECT_SELECT::get_next()
{
  List_iterator_fast<optimizer::QuickRangeSelect> quick_it(quick_selects);
  optimizer::QuickRangeSelect *quick= NULL;
  int error;
  int  cmp;
  uint32_t last_rowid_count= 0;

  do
  {
    /* Get a rowid for first quick and save it as a 'candidate' */
    quick= quick_it++;
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

    while (last_rowid_count < quick_selects.elements)
    {
      if (! (quick= quick_it++))
      {
        quick_it.rewind();
        quick= quick_it++;
      }

      do
      {
        if ((error= quick->get_next()))
          return(error);
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


void optimizer::QUICK_ROR_INTERSECT_SELECT::add_info_string(String *str)
{
  bool first= true;
  optimizer::QuickRangeSelect *quick= NULL;
  List_iterator_fast<optimizer::QuickRangeSelect> it(quick_selects);
  str->append(STRING_WITH_LEN("intersect("));
  while ((quick= it++))
  {
    KEY *key_info= head->key_info + quick->index;
    if (! first)
      str->append(',');
    else
      first= false;
    str->append(key_info->name);
  }
  if (cpk_quick)
  {
    KEY *key_info= head->key_info + cpk_quick->index;
    str->append(',');
    str->append(key_info->name);
  }
  str->append(')');
}


void optimizer::QUICK_ROR_INTERSECT_SELECT::add_keys_and_lengths(String *key_names,
                                                                 String *used_lengths)
{
  char buf[64];
  uint32_t length;
  bool first= true;
  optimizer::QuickRangeSelect *quick;
  List_iterator_fast<optimizer::QuickRangeSelect> it(quick_selects);
  while ((quick= it++))
  {
    KEY *key_info= head->key_info + quick->index;
    if (first)
    {
      first= false;
    }
    else
    {
      key_names->append(',');
      used_lengths->append(',');
    }
    key_names->append(key_info->name);
    length= int64_t2str(quick->max_used_key_length, buf, 10) - buf;
    used_lengths->append(buf, length);
  }

  if (cpk_quick)
  {
    KEY *key_info= head->key_info + cpk_quick->index;
    key_names->append(',');
    key_names->append(key_info->name);
    length= int64_t2str(cpk_quick->max_used_key_length, buf, 10) - buf;
    used_lengths->append(',');
    used_lengths->append(buf, length);
  }
}

