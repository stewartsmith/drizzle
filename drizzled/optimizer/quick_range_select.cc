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
#include <drizzled/optimizer/quick_range.h>
#include <drizzled/optimizer/quick_range_select.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/current_session.h>
#include <drizzled/key.h>
#include <drizzled/table.h>
#include <drizzled/util/test.h>
#include <drizzled/system_variables.h>

#include <fcntl.h>

using namespace std;

namespace drizzled {


optimizer::QuickRangeSelect::QuickRangeSelect(Session *session,
                                              Table *table,
                                              uint32_t key_nr,
                                              bool no_alloc,
                                              memory::Root *parent_alloc)
  :
    cursor(NULL),
    ranges(),
    in_ror_merged_scan(false),
    column_bitmap(NULL),
    save_read_set(NULL),
    save_write_set(NULL),
    free_file(false),
    cur_range(NULL),
    last_range(NULL),
    qr_traversal_ctx(),
    mrr_buf_size(0),
    key_parts(NULL),
    dont_free(false),
    mrr_flags(0),
    alloc()
{
  sorted= 0;
  index= key_nr;
  head= table;
  key_part_info= head->key_info[index].key_part;
  my_init_dynamic_array(&ranges, sizeof(optimizer::QuickRange*), 16, 16);

  /* 'session' is not accessible in QuickRangeSelect::reset(). */
  mrr_buf_size= session->variables.read_rnd_buff_size;

  if (! no_alloc && ! parent_alloc)
  {
    // Allocates everything through the internal memroot
    alloc.init(session->variables.range_alloc_block_size);
    session->mem_root= &alloc;
  }
  else
  {
    memset(&alloc, 0, sizeof(alloc));
  }

  cursor= head->cursor;
  record= head->record[0];
  save_read_set= head->read_set;
  save_write_set= head->write_set;
  column_bitmap= new boost::dynamic_bitset<>(table->getShare()->sizeFields());
}


int optimizer::QuickRangeSelect::init()
{
  if (cursor->inited != Cursor::NONE)
    cursor->ha_index_or_rnd_end();
  return (cursor->startIndexScan(index, 1));
}


void optimizer::QuickRangeSelect::range_end()
{
  if (cursor->inited != Cursor::NONE)
    cursor->ha_index_or_rnd_end();
}


optimizer::QuickRangeSelect::~QuickRangeSelect()
{
  if (! dont_free)
  {
    /* cursor is NULL for CPK scan on covering ROR-intersection */
    if (cursor)
    {
      range_end();
      if (head->key_read)
      {
        head->key_read= 0;
        cursor->extra(HA_EXTRA_NO_KEYREAD);
      }
      if (free_file)
      {
        cursor->ha_external_lock(current_session, F_UNLCK);
        cursor->close();
        delete cursor;
      }
    }
    delete_dynamic(&ranges); /* ranges are allocated in alloc */
    delete column_bitmap;
    alloc.free_root(MYF(0));
  }
  head->column_bitmaps_set(*save_read_set, *save_write_set);
}


int optimizer::QuickRangeSelect::init_ror_merged_scan(bool reuse_handler)
{
  Cursor *save_file= cursor, *org_file;
  Session *session;

  in_ror_merged_scan= 1;
  if (reuse_handler)
  {
    if (init() || reset())
    {
      return 0;
    }
    head->column_bitmaps_set(*column_bitmap, *column_bitmap);
    goto end;
  }

  /* Create a separate Cursor object for this quick select */
  if (free_file)
  {
    /* already have own 'Cursor' object. */
    return 0;
  }

  session= head->in_use;
  if (! (cursor= head->cursor->clone(session->mem_root)))
  {
    /*
      Manually set the error flag. Note: there seems to be quite a few
      places where a failure could cause the server to "hang" the client by
      sending no response to a query. ATM those are not real errors because
      the storage engine calls in question happen to never fail with the
      existing storage engines.
    */
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    /* Caller will free the memory */
    goto failure;
  }

  head->column_bitmaps_set(*column_bitmap, *column_bitmap);

  if (cursor->ha_external_lock(session, F_RDLCK))
    goto failure;

  if (init() || reset())
  {
    cursor->ha_external_lock(session, F_UNLCK);
    cursor->close();
    goto failure;
  }
  free_file= true;
  last_rowid= cursor->ref;

end:
  /*
    We are only going to read key fields and call position() on 'cursor'
    The following sets head->tmp_set to only use this key and then updates
    head->read_set and head->write_set to use this bitmap.
    The now bitmap is stored in 'column_bitmap' which is used in ::get_next()
  */
  org_file= head->cursor;
  head->cursor= cursor;
  /* We don't have to set 'head->keyread' here as the 'cursor' is unique */
  if (! head->no_keyread)
  {
    head->key_read= 1;
    head->mark_columns_used_by_index(index);
  }
  head->prepare_for_position();
  head->cursor= org_file;
  *column_bitmap|= *head->read_set;
  head->column_bitmaps_set(*column_bitmap, *column_bitmap);

  return 0;

failure:
  head->column_bitmaps_set(*save_read_set, *save_write_set);
  delete cursor;
  cursor= save_file;
  return 0;
}


void optimizer::QuickRangeSelect::save_last_pos()
{
  cursor->position(record);
}


bool optimizer::QuickRangeSelect::unique_key_range() const
{
  if (ranges.size() == 1)
  {
    optimizer::QuickRange *tmp= *((optimizer::QuickRange**)ranges.buffer);
    if ((tmp->flag & (EQ_RANGE | NULL_RANGE)) == EQ_RANGE)
    {
      KeyInfo *key=head->key_info+index;
      return ((key->flags & (HA_NOSAME)) == HA_NOSAME &&
	      key->key_length == tmp->min_length);
    }
  }
  return false;
}


int optimizer::QuickRangeSelect::reset()
{
  int error= 0;
  last_range= NULL;
  cur_range= (optimizer::QuickRange**) ranges.buffer;

  if (cursor->inited == Cursor::NONE && (error= cursor->startIndexScan(index, 1)))
  {
    return error;
  }

  /*
    (in the past) Allocate buffer if we need one but haven't allocated it yet 
    There is a later assert in th code that hoped to catch random free() that might
    have done this.
  */
  assert(not (mrr_buf_size));

  if (sorted)
  {
     mrr_flags|= HA_MRR_SORTED;
  }
  RANGE_SEQ_IF seq_funcs= {
    optimizer::quick_range_seq_init,
    optimizer::quick_range_seq_next
  };
  error= cursor->multi_range_read_init(&seq_funcs,
                                       (void*) this,
                                       ranges.size(),
                                       mrr_flags);
  return error;
}


int optimizer::QuickRangeSelect::get_next()
{
  char *dummy= NULL;
  if (in_ror_merged_scan)
  {
    /*
      We don't need to signal the bitmap change as the bitmap is always the
      same for this head->cursor
    */
    head->column_bitmaps_set(*column_bitmap, *column_bitmap);
  }

  int result= cursor->multi_range_read_next(&dummy);

  if (in_ror_merged_scan)
  {
    /* Restore bitmaps set on entry */
    head->column_bitmaps_set(*save_read_set, *save_write_set);
  }
  return result;
}


int optimizer::QuickRangeSelect::get_next_prefix(uint32_t prefix_length,
                                                 key_part_map keypart_map,
                                                 unsigned char *cur_prefix)
{
  for (;;)
  {
    int result;
    key_range start_key, end_key;
    if (last_range)
    {
      /* Read the next record in the same range with prefix after cur_prefix. */
      assert(cur_prefix != 0);
      result= cursor->index_read_map(record,
                                     cur_prefix,
                                     keypart_map,
                                     HA_READ_AFTER_KEY);
      if (result || (cursor->compare_key(cursor->end_range) <= 0))
        return result;
    }

    uint32_t count= ranges.size() - (cur_range - (optimizer::QuickRange**) ranges.buffer);
    if (count == 0)
    {
      /* Ranges have already been used up before. None is left for read. */
      last_range= 0;
      return HA_ERR_END_OF_FILE;
    }
    last_range= *(cur_range++);

    start_key.key= (const unsigned char*) last_range->min_key;
    start_key.length= min(last_range->min_length, (uint16_t)prefix_length);
    start_key.keypart_map= last_range->min_keypart_map & keypart_map;
    start_key.flag= ((last_range->flag & NEAR_MIN) ? HA_READ_AFTER_KEY :
		                                                (last_range->flag & EQ_RANGE) ?
		                                                HA_READ_KEY_EXACT : HA_READ_KEY_OR_NEXT);
    end_key.key= (const unsigned char*) last_range->max_key;
    end_key.length= min(last_range->max_length, (uint16_t)prefix_length);
    end_key.keypart_map= last_range->max_keypart_map & keypart_map;
    /*
      We use READ_AFTER_KEY here because if we are reading on a key
      prefix we want to find all keys with this prefix
    */
    end_key.flag= (last_range->flag & NEAR_MAX ? HA_READ_BEFORE_KEY :
		                                             HA_READ_AFTER_KEY);

    result= cursor->read_range_first(last_range->min_keypart_map ? &start_key : 0,
				                             last_range->max_keypart_map ? &end_key : 0,
                                     test(last_range->flag & EQ_RANGE),
				                             sorted);
    if (last_range->flag == (UNIQUE_RANGE | EQ_RANGE))
      last_range= 0; // Stop searching

    if (result != HA_ERR_END_OF_FILE)
      return result;
    last_range= 0; // No matching rows; go to next range
  }
}


bool optimizer::QuickRangeSelect::row_in_ranges()
{
  optimizer::QuickRange *res= NULL;
  uint32_t min= 0;
  uint32_t max= ranges.size() - 1;
  uint32_t mid= (max + min) / 2;

  while (min != max)
  {
    if (cmp_next(reinterpret_cast<optimizer::QuickRange**>(ranges.buffer)[mid]))
    {
      /* current row value > mid->max */
      min= mid + 1;
    }
    else
      max= mid;
    mid= (min + max) / 2;
  }
  res= reinterpret_cast<optimizer::QuickRange**>(ranges.buffer)[mid];
  return not cmp_next(res) && not cmp_prev(res);
}


int optimizer::QuickRangeSelect::cmp_next(optimizer::QuickRange *range_arg)
{
  if (range_arg->flag & NO_MAX_RANGE)
    return 0;                                   /* key can't be to large */

  KEY_PART *key_part= key_parts;
  uint32_t store_length;

  for (unsigned char *key=range_arg->max_key, *end=key+range_arg->max_length;
       key < end;
       key+= store_length, key_part++)
  {
    int cmp;
    store_length= key_part->store_length;
    if (key_part->null_bit)
    {
      if (*key)
      {
        if (! key_part->field->is_null())
          return 1;
        continue;
      }
      else if (key_part->field->is_null())
        return 0;
      key++;					// Skip null byte
      store_length--;
    }
    if ((cmp= key_part->field->key_cmp(key, key_part->length)) < 0)
      return 0;
    if (cmp > 0)
      return 1;
  }
  return (range_arg->flag & NEAR_MAX) ? 1 : 0;          // Exact match
}


int optimizer::QuickRangeSelect::cmp_prev(optimizer::QuickRange *range_arg)
{
  if (range_arg->flag & NO_MIN_RANGE)
    return 0; /* key can't be to small */

  int cmp= key_cmp(key_part_info,
                   range_arg->min_key,
                   range_arg->min_length);
  if (cmp > 0 || (cmp == 0 && (range_arg->flag & NEAR_MIN) == false))
    return 0;
  return 1; // outside of range
}


void optimizer::QuickRangeSelect::add_info_string(string *str)
{
  KeyInfo *key_info= head->key_info + index;
  str->append(key_info->name);
}


void optimizer::QuickRangeSelect::add_keys_and_lengths(string *key_names,
                                                       string *used_lengths)
{
  char buf[64];
  uint32_t length;
  KeyInfo *key_info= head->key_info + index;
  key_names->append(key_info->name);
  length= internal::int64_t2str(max_used_key_length, buf, 10) - buf;
  used_lengths->append(buf, length);
}


/*
  This is a hack: we inherit from QUICK_SELECT so that we can use the
  get_next() interface, but we have to hold a pointer to the original
  QUICK_SELECT because its data are used all over the place.  What
  should be done is to factor out the data that is needed into a base
  class (QUICK_SELECT), and then have two subclasses (_ASC and _DESC)
  which handle the ranges and implement the get_next() function.  But
  for now, this seems to work right at least.
 */
optimizer::QuickSelectDescending::QuickSelectDescending(optimizer::QuickRangeSelect *q, uint32_t, bool *)
  :
    optimizer::QuickRangeSelect(*q)
{
  optimizer::QuickRange **pr= (optimizer::QuickRange**) ranges.buffer;
  optimizer::QuickRange **end_range= pr + ranges.size();
  for (; pr != end_range; pr++)
  {
    rev_ranges.push_back(*pr);
  }
  rev_it= rev_ranges.begin();

  /* Remove EQ_RANGE flag for keys that are not using the full key */
  BOOST_FOREACH(QuickRange* it, rev_ranges)
  {
    if ((it->flag & EQ_RANGE) && head->key_info[index].key_length != it->max_length)
    {
      it->flag&= ~EQ_RANGE;
    }
  }
  q->dont_free= 1; // Don't free shared mem
  delete q;
}


int optimizer::QuickSelectDescending::get_next()
{
  /* The max key is handled as follows:
   *   - if there is NO_MAX_RANGE, start at the end and move backwards
   *   - if it is an EQ_RANGE, which means that max key covers the entire
   *     key, go directly to the key and read through it (sorting backwards is
   *     same as sorting forwards)
   *   - if it is NEAR_MAX, go to the key or next, step back once, and
   *     move backwards
   *   - otherwise (not NEAR_MAX == include the key), go after the key,
   *     step back once, and move backwards
   */
  for (;;)
  {
    int result;
    if (last_range)
    {						// Already read through key
      result= ((last_range->flag & EQ_RANGE) ?
		           cursor->index_next_same(record, last_range->min_key,
					                             last_range->min_length) :
		           cursor->index_prev(record));
      if (! result)
      {
          if (cmp_prev(*(rev_it - 1)) == 0)
            return 0;
      }
      else if (result != HA_ERR_END_OF_FILE)
        return result;
    }

    if (rev_it == rev_ranges.end())
    {
      return HA_ERR_END_OF_FILE; // All ranges used
    }
    last_range= *rev_it;
    ++rev_it;

    if (last_range->flag & NO_MAX_RANGE)        // Read last record
    {
      int local_error;
      if ((local_error= cursor->index_last(record)))
        return local_error;	// Empty table
      if (cmp_prev(last_range) == 0)
        return 0;
      last_range= 0; // No match; go to next range
      continue;
    }

    if (last_range->flag & EQ_RANGE)
    {
      result = cursor->index_read_map(record,
                                      last_range->max_key,
                                      last_range->max_keypart_map,
                                      HA_READ_KEY_EXACT);
    }
    else
    {
      assert(last_range->flag & NEAR_MAX ||
             range_reads_after_key(last_range));
      result= cursor->index_read_map(record,
                                     last_range->max_key,
                                     last_range->max_keypart_map,
                                     ((last_range->flag & NEAR_MAX) ?
                                      HA_READ_BEFORE_KEY :
                                      HA_READ_PREFIX_LAST_OR_PREV));
    }
    if (result)
    {
      if (result != HA_ERR_KEY_NOT_FOUND && result != HA_ERR_END_OF_FILE)
        return result;
      last_range= 0;                            // Not found, to next range
      continue;
    }
    if (cmp_prev(last_range) == 0)
    {
      if (last_range->flag == (UNIQUE_RANGE | EQ_RANGE))
        last_range= 0;				// Stop searching
      return 0;				// Found key is in range
    }
    last_range= 0;                              // To next range
  }
}


/*
 * true if this range will require using HA_READ_AFTER_KEY
   See comment in get_next() about this
 */
bool optimizer::QuickSelectDescending::range_reads_after_key(optimizer::QuickRange *range_arg)
{
  return ((range_arg->flag & (NO_MAX_RANGE | NEAR_MAX)) ||
	        ! (range_arg->flag & EQ_RANGE) ||
	        head->key_info[index].key_length != range_arg->max_length) ? 1 : 0;
}


} /* namespace drizzled */
