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
#include <drizzled/sql_select.h>
#include <drizzled/join.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/optimizer/quick_group_min_max_select.h>
#include <drizzled/optimizer/quick_range.h>
#include <drizzled/optimizer/quick_range_select.h>
#include <drizzled/optimizer/sel_arg.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/util/functors.h>
#include <drizzled/key.h>
#include <drizzled/table.h>
#include <drizzled/system_variables.h>

#include <vector>

using namespace std;

namespace drizzled {
namespace optimizer {

QuickGroupMinMaxSelect::QuickGroupMinMaxSelect(Table *table,
                       Join *join_arg,
                       bool have_min_arg,
                       bool have_max_arg,
                       KeyPartInfo *min_max_arg_part_arg,
                       uint32_t group_prefix_len_arg,
                       uint32_t group_key_parts_arg,
                       uint32_t used_key_parts_arg,
                       KeyInfo *index_info_arg,
                       uint32_t use_index,
                       double read_cost_arg,
                       ha_rows records_arg,
                       uint32_t key_infix_len_arg,
                       unsigned char *key_infix_arg,
                       memory::Root *parent_alloc)
  :
    join(join_arg),
    index_info(index_info_arg),
    group_prefix_len(group_prefix_len_arg),
    group_key_parts(group_key_parts_arg),
    have_min(have_min_arg),
    have_max(have_max_arg),
    seen_first_key(false),
    min_max_arg_part(min_max_arg_part_arg),
    key_infix(key_infix_arg),
    key_infix_len(key_infix_len_arg),
    min_functions_it(NULL),
    max_functions_it(NULL)
{
  head= table;
  cursor= head->cursor;
  index= use_index;
  record= head->record[0];
  tmp_record= head->getUpdateRecord();
  read_time= read_cost_arg;
  records= records_arg;
  used_key_parts= used_key_parts_arg;
  real_key_parts= used_key_parts_arg;
  real_prefix_len= group_prefix_len + key_infix_len;
  group_prefix= NULL;
  min_max_arg_len= min_max_arg_part ? min_max_arg_part->store_length : 0;

  /*
    We can't have parent_alloc set as the init function can't handle this case
    yet.
  */
  assert(! parent_alloc);
  if (! parent_alloc)
  {
    alloc.init(join->session->variables.range_alloc_block_size);
    join->session->mem_root= &alloc;
  }
  else
    memset(&alloc, 0, sizeof(memory::Root));  // ensure that it's not used
}


int QuickGroupMinMaxSelect::init()
{
  if (group_prefix) /* Already initialized. */
    return 0;

  last_prefix= alloc.alloc(group_prefix_len);
  /*
    We may use group_prefix to store keys with all select fields, so allocate
    enough space for it.
  */
  group_prefix= alloc.alloc(real_prefix_len + min_max_arg_len);

  if (key_infix_len > 0)
  {
    /*
      The memory location pointed to by key_infix will be deleted soon, so
      allocate a new buffer and copy the key_infix into it.
    */
    unsigned char *tmp_key_infix= alloc.alloc(key_infix_len);
    memcpy(tmp_key_infix, this->key_infix, key_infix_len);
    this->key_infix= tmp_key_infix;
  }

  if (min_max_arg_part)
  {
    min_functions= have_min ? new List<Item_sum> : NULL;
    max_functions= have_max ? new List<Item_sum> : NULL;
    Item_sum **func_ptr= join->sum_funcs;
    while (Item_sum* min_max_item= *(func_ptr++))
    {
      if (have_min && (min_max_item->sum_func() == Item_sum::MIN_FUNC))
        min_functions->push_back(min_max_item);
      else if (have_max && (min_max_item->sum_func() == Item_sum::MAX_FUNC))
        max_functions->push_back(min_max_item);
    }

    if (have_min)
      min_functions_it= new List<Item_sum>::iterator(min_functions->begin());
    if (have_max)
      max_functions_it= new List<Item_sum>::iterator(max_functions->begin());
  }
  return 0;
}

QuickGroupMinMaxSelect::~QuickGroupMinMaxSelect()
{
  if (cursor->inited != Cursor::NONE)
  {
    cursor->endIndexScan();
  }
  if (min_max_arg_part)
  {
    for_each(min_max_ranges.begin(),
             min_max_ranges.end(),
             DeletePtr());
  }
  min_max_ranges.clear();
  alloc.free_root(MYF(0));
  delete min_functions_it;
  delete max_functions_it;
  delete quick_prefix_select;
}


bool QuickGroupMinMaxSelect::add_range(SEL_ARG *sel_range)
{
  QuickRange *range= NULL;
  uint32_t range_flag= sel_range->min_flag | sel_range->max_flag;

  /* Skip (-inf,+inf) ranges, e.g. (x < 5 or x > 4). */
  if ((range_flag & NO_MIN_RANGE) && (range_flag & NO_MAX_RANGE))
    return false;

  if (! (sel_range->min_flag & NO_MIN_RANGE) &&
      ! (sel_range->max_flag & NO_MAX_RANGE))
  {
    if (sel_range->maybe_null &&
        sel_range->min_value[0] && sel_range->max_value[0])
      range_flag|= NULL_RANGE; /* IS NULL condition */
    else if (memcmp(sel_range->min_value, sel_range->max_value,
                    min_max_arg_len) == 0)
      range_flag|= EQ_RANGE;  /* equality condition */
  }
  range= new QuickRange(sel_range->min_value,
                                   min_max_arg_len,
                                   make_keypart_map(sel_range->part),
                                   sel_range->max_value,
                                   min_max_arg_len,
                                   make_keypart_map(sel_range->part),
                                   range_flag);
  if (! range)
    return true;
  min_max_ranges.push_back(range);
  return false;
}


void QuickGroupMinMaxSelect::adjust_prefix_ranges()
{
  if (quick_prefix_select &&
      group_prefix_len < quick_prefix_select->max_used_key_length)
  {
    DYNAMIC_ARRAY& arr= quick_prefix_select->ranges;
    for (size_t inx= 0; inx < arr.size(); inx++)
      reinterpret_cast<QuickRange**>(arr.buffer)[inx]->flag &= ~(NEAR_MIN | NEAR_MAX);
  }
}


void QuickGroupMinMaxSelect::update_key_stat()
{
  max_used_key_length= real_prefix_len;
  if (! min_max_ranges.empty())
  {
    QuickRange *cur_range= NULL;
    if (have_min)
    { /* Check if the right-most range has a lower boundary. */
      cur_range= min_max_ranges.back();
      if (! (cur_range->flag & NO_MIN_RANGE))
      {
        max_used_key_length+= min_max_arg_len;
        used_key_parts++;
        return;
      }
    }
    if (have_max)
    { /* Check if the left-most range has an upper boundary. */
      cur_range= min_max_ranges.front();
      if (! (cur_range->flag & NO_MAX_RANGE))
      {
        max_used_key_length+= min_max_arg_len;
        used_key_parts++;
        return;
      }
    }
  }
  else if (have_min && min_max_arg_part &&
           min_max_arg_part->field->real_maybe_null())
  {
    /*
      If a MIN/MAX argument value is NULL, we can quickly determine
      that we're in the beginning of the next group, because NULLs
      are always < any other value. This allows us to quickly
      determine the end of the current group and jump to the next
      group (see next_min()) and thus effectively increases the
      usable key length.
    */
    max_used_key_length+= min_max_arg_len;
    used_key_parts++;
  }
}


int QuickGroupMinMaxSelect::reset(void)
{
  int result;

  cursor->extra(HA_EXTRA_KEYREAD); /* We need only the key attributes */
  if ((result= cursor->startIndexScan(index,1)))
    return result;
  if (quick_prefix_select && quick_prefix_select->reset())
    return 0;
  result= cursor->index_last(record);
  if (result == HA_ERR_END_OF_FILE)
    return 0;
  /* Save the prefix of the last group. */
  key_copy(last_prefix, record, index_info, group_prefix_len);

  return 0;
}


int QuickGroupMinMaxSelect::get_next()
{
  int min_res= 0;
  int max_res= 0;
  int result= 0;
  int is_last_prefix= 0;

  /*
    Loop until a group is found that satisfies all query conditions or the last
    group is reached.
  */
  do
  {
    result= next_prefix();
    /*
      Check if this is the last group prefix. Notice that at this point
      this->record contains the current prefix in record format.
    */
    if (! result)
    {
      is_last_prefix= key_cmp(index_info->key_part, last_prefix,
                              group_prefix_len);
      assert(is_last_prefix <= 0);
    }
    else
    {
      if (result == HA_ERR_KEY_NOT_FOUND)
        continue;
      break;
    }

    if (have_min)
    {
      min_res= next_min();
      if (min_res == 0)
        update_min_result();
    }
    /* If there is no MIN in the group, there is no MAX either. */
    if ((have_max && !have_min) ||
        (have_max && have_min && (min_res == 0)))
    {
      max_res= next_max();
      if (max_res == 0)
        update_max_result();
      /* If a MIN was found, a MAX must have been found as well. */
      assert(((have_max && !have_min) ||
                  (have_max && have_min && (max_res == 0))));
    }
    /*
      If this is just a GROUP BY or DISTINCT without MIN or MAX and there
      are equality predicates for the key parts after the group, find the
      first sub-group with the extended prefix.
    */
    if (! have_min && ! have_max && key_infix_len > 0)
      result= cursor->index_read_map(record,
                                     group_prefix,
                                     make_prev_keypart_map(real_key_parts),
                                     HA_READ_KEY_EXACT);

    result= have_min ? min_res : have_max ? max_res : result;
  } while ((result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE) &&
           is_last_prefix != 0);

  if (result == 0)
  {
    /*
      Partially mimic the behavior of end_select_send. Copy the
      field data from Item_field::field into Item_field::result_field
      of each non-aggregated field (the group fields, and optionally
      other fields in non-ANSI SQL mode).
    */
    copy_fields(&join->tmp_table_param);
  }
  else if (result == HA_ERR_KEY_NOT_FOUND)
    result= HA_ERR_END_OF_FILE;

  return result;
}


int QuickGroupMinMaxSelect::next_min()
{
  int result= 0;

  /* Find the MIN key using the eventually extended group prefix. */
  if (! min_max_ranges.empty())
  {
    if ((result= next_min_in_range()))
      return result;
  }
  else
  {
    /* Apply the constant equality conditions to the non-group select fields */
    if (key_infix_len > 0)
    {
      if ((result= cursor->index_read_map(record,
                                          group_prefix,
                                          make_prev_keypart_map(real_key_parts),
                                          HA_READ_KEY_EXACT)))
        return result;
    }

    /*
      If the min/max argument field is NULL, skip subsequent rows in the same
      group with NULL in it. Notice that:
      - if the first row in a group doesn't have a NULL in the field, no row
      in the same group has (because NULL < any other value),
      - min_max_arg_part->field->ptr points to some place in 'record'.
    */
    if (min_max_arg_part && min_max_arg_part->field->is_null())
    {
      /* Find the first subsequent record without NULL in the MIN/MAX field. */
      key_copy(tmp_record, record, index_info, 0);
      result= cursor->index_read_map(record,
                                     tmp_record,
                                     make_keypart_map(real_key_parts),
                                     HA_READ_AFTER_KEY);
      /*
        Check if the new record belongs to the current group by comparing its
        prefix with the group's prefix. If it is from the next group, then the
        whole group has NULLs in the MIN/MAX field, so use the first record in
        the group as a result.
        TODO:
        It is possible to reuse this new record as the result candidate for the
        next call to next_min(), and to save one lookup in the next call. For
        this add a new member 'this->next_group_prefix'.
      */
      if (! result)
      {
        if (key_cmp(index_info->key_part, group_prefix, real_prefix_len))
          key_restore(record, tmp_record, index_info, 0);
      }
      else if (result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE)
        result= 0; /* There is a result in any case. */
    }
  }

  /*
    If the MIN attribute is non-nullable, this->record already contains the
    MIN key in the group, so just return.
  */
  return result;
}


int QuickGroupMinMaxSelect::next_max()
{
  /* Get the last key in the (possibly extended) group. */
  return min_max_ranges.empty()
    ? cursor->index_read_map(record, group_prefix, make_prev_keypart_map(real_key_parts), HA_READ_PREFIX_LAST)
    : next_max_in_range();
}


int QuickGroupMinMaxSelect::next_prefix()
{
  int result= 0;

  if (quick_prefix_select)
  {
    unsigned char *cur_prefix= seen_first_key ? group_prefix : NULL;
    if ((result= quick_prefix_select->get_next_prefix(group_prefix_len,
                                                      make_prev_keypart_map(group_key_parts),
                                                      cur_prefix)))
      return result;
    seen_first_key= true;
  }
  else
  {
    if (! seen_first_key)
    {
      result= cursor->index_first(record);
      if (result)
        return result;
      seen_first_key= true;
    }
    else
    {
      /* Load the first key in this group into record. */
      result= cursor->index_read_map(record,
                                     group_prefix,
                                     make_prev_keypart_map(group_key_parts),
                                     HA_READ_AFTER_KEY);
      if (result)
        return result;
    }
  }

  /* Save the prefix of this group for subsequent calls. */
  key_copy(group_prefix, record, index_info, group_prefix_len);
  /* Append key_infix to group_prefix. */
  if (key_infix_len > 0)
    memcpy(group_prefix + group_prefix_len,
           key_infix,
           key_infix_len);

  return 0;
}


int QuickGroupMinMaxSelect::next_min_in_range()
{
  ha_rkey_function find_flag;
  key_part_map keypart_map;
  QuickRange *cur_range= NULL;
  bool found_null= false;
  int result= HA_ERR_KEY_NOT_FOUND;
  basic_string<unsigned char> max_key;

  max_key.reserve(real_prefix_len + min_max_arg_len);

  assert(! min_max_ranges.empty());

  for (vector<QuickRange *>::iterator it= min_max_ranges.begin(); it != min_max_ranges.end(); ++it)
  { /* Search from the left-most range to the right. */
    cur_range= *it;

    /*
      If the current value for the min/max argument is bigger than the right
      boundary of cur_range, there is no need to check this range.
    */
    if (it != min_max_ranges.begin() && 
        ! (cur_range->flag & NO_MAX_RANGE) &&
        (key_cmp(min_max_arg_part,
                 (const unsigned char*) cur_range->max_key,
                 min_max_arg_len) == 1))
      continue;

    if (cur_range->flag & NO_MIN_RANGE)
    {
      keypart_map= make_prev_keypart_map(real_key_parts);
      find_flag= HA_READ_KEY_EXACT;
    }
    else
    {
      /* Extend the search key with the lower boundary for this range. */
      memcpy(group_prefix + real_prefix_len,
             cur_range->min_key,
             cur_range->min_length);
      keypart_map= make_keypart_map(real_key_parts);
      find_flag= (cur_range->flag & (EQ_RANGE | NULL_RANGE)) ?
                 HA_READ_KEY_EXACT : (cur_range->flag & NEAR_MIN) ?
                 HA_READ_AFTER_KEY : HA_READ_KEY_OR_NEXT;
    }

    result= cursor->index_read_map(record, group_prefix, keypart_map, find_flag);
    if (result)
    {
      if ((result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE) &&
          (cur_range->flag & (EQ_RANGE | NULL_RANGE)))
        continue; /* Check the next range. */

      /*
        In all other cases (HA_ERR_*, HA_READ_KEY_EXACT with NO_MIN_RANGE,
        HA_READ_AFTER_KEY, HA_READ_KEY_OR_NEXT) if the lookup failed for this
        range, it can't succeed for any other subsequent range.
      */
      break;
    }

    /* A key was found. */
    if (cur_range->flag & EQ_RANGE)
      break; /* No need to perform the checks below for equal keys. */

    if (cur_range->flag & NULL_RANGE)
    {
      /*
        Remember this key, and continue looking for a non-NULL key that
        satisfies some other condition.
      */
      memcpy(tmp_record, record, head->getShare()->rec_buff_length);
      found_null= true;
      continue;
    }

    /* Check if record belongs to the current group. */
    if (key_cmp(index_info->key_part, group_prefix, real_prefix_len))
    {
      result= HA_ERR_KEY_NOT_FOUND;
      continue;
    }

    /* If there is an upper limit, check if the found key is in the range. */
    if (! (cur_range->flag & NO_MAX_RANGE) )
    {
      /* Compose the MAX key for the range. */
      max_key.clear();
      max_key.append(group_prefix, real_prefix_len);
      max_key.append(cur_range->max_key, cur_range->max_length);
      /* Compare the found key with max_key. */
      int cmp_res= key_cmp(index_info->key_part,
                           max_key.data(),
                           real_prefix_len + min_max_arg_len);
      if (! (((cur_range->flag & NEAR_MAX) && (cmp_res == -1)) ||
          (cmp_res <= 0)))
      {
        result= HA_ERR_KEY_NOT_FOUND;
        continue;
      }
    }
    /* If we got to this point, the current key qualifies as MIN. */
    assert(result == 0);
    break;
  }
  /*
    If there was a key with NULL in the MIN/MAX field, and there was no other
    key without NULL from the same group that satisfies some other condition,
    then use the key with the NULL.
  */
  if (found_null && result)
  {
    memcpy(record, tmp_record, head->getShare()->rec_buff_length);
    result= 0;
  }
  return result;
}


int QuickGroupMinMaxSelect::next_max_in_range()
{
  ha_rkey_function find_flag;
  key_part_map keypart_map;
  QuickRange *cur_range= NULL;
  int result= 0;
  basic_string<unsigned char> min_key;
  min_key.reserve(real_prefix_len + min_max_arg_len);

  assert(! min_max_ranges.empty());

  for (vector<QuickRange *>::reverse_iterator rit= min_max_ranges.rbegin();
       rit != min_max_ranges.rend();
       ++rit)
  { /* Search from the right-most range to the left. */
    cur_range= *rit;

    /*
      If the current value for the min/max argument is smaller than the left
      boundary of cur_range, there is no need to check this range.
    */
    if (rit != min_max_ranges.rbegin() &&
        ! (cur_range->flag & NO_MIN_RANGE) &&
        (key_cmp(min_max_arg_part,
                 (const unsigned char*) cur_range->min_key,
                 min_max_arg_len) == -1))
      continue;

    if (cur_range->flag & NO_MAX_RANGE)
    {
      keypart_map= make_prev_keypart_map(real_key_parts);
      find_flag= HA_READ_PREFIX_LAST;
    }
    else
    {
      /* Extend the search key with the upper boundary for this range. */
      memcpy(group_prefix + real_prefix_len,
             cur_range->max_key,
             cur_range->max_length);
      keypart_map= make_keypart_map(real_key_parts);
      find_flag= (cur_range->flag & EQ_RANGE) ?
                 HA_READ_KEY_EXACT : (cur_range->flag & NEAR_MAX) ?
                 HA_READ_BEFORE_KEY : HA_READ_PREFIX_LAST_OR_PREV;
    }

    result= cursor->index_read_map(record, group_prefix, keypart_map, find_flag);

    if (result)
    {
      if ((result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE) &&
          (cur_range->flag & EQ_RANGE))
        continue; /* Check the next range. */

      /*
        In no key was found with this upper bound, there certainly are no keys
        in the ranges to the left.
      */
      return result;
    }
    /* A key was found. */
    if (cur_range->flag & EQ_RANGE)
      return 0; /* No need to perform the checks below for equal keys. */

    /* Check if record belongs to the current group. */
    if (key_cmp(index_info->key_part, group_prefix, real_prefix_len))
      continue;                                 // Row not found

    /* If there is a lower limit, check if the found key is in the range. */
    if (! (cur_range->flag & NO_MIN_RANGE) )
    {
      /* Compose the MIN key for the range. */
      min_key.clear();
      min_key.append(group_prefix, real_prefix_len);
      min_key.append(cur_range->min_key, cur_range->min_length);

      /* Compare the found key with min_key. */
      int cmp_res= key_cmp(index_info->key_part,
                           min_key.data(),
                           real_prefix_len + min_max_arg_len);
      if (! (((cur_range->flag & NEAR_MIN) && (cmp_res == 1)) ||
          (cmp_res >= 0)))
        continue;
    }
    /* If we got to this point, the current key qualifies as MAX. */
    return result;
  }
  return HA_ERR_KEY_NOT_FOUND;
}


void QuickGroupMinMaxSelect::update_min_result()
{
  *min_functions_it= min_functions->begin();
  for (Item_sum *min_func; (min_func= (*min_functions_it)++); )
    min_func->reset();
}


void QuickGroupMinMaxSelect::update_max_result()
{
  *max_functions_it= max_functions->begin();
  for (Item_sum *max_func; (max_func= (*max_functions_it)++); )
    max_func->reset();
}


void QuickGroupMinMaxSelect::add_keys_and_lengths(string *key_names,
                                                             string *used_lengths)
{
  char buf[64];
  key_names->append(index_info->name);
  uint32_t length= internal::int64_t2str(max_used_key_length, buf, 10) - buf;
  used_lengths->append(buf, length);
}

}
} /* namespace drizzled */
