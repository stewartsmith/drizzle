
#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/sql_select.h"
#include "drizzled/join.h"
#include "drizzled/optimizer/range.h"
#include "drizzled/optimizer/quick_group_min_max_select.h"
#include "drizzled/optimizer/quick_range.h"
#include "drizzled/optimizer/quick_range_select.h"
#include "drizzled/optimizer/sel_arg.h"

using namespace std;
using namespace drizzled;


/*
  Construct new quick select for group queries with min/max.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::QUICK_GROUP_MIN_MAX_SELECT()
    table             The table being accessed
    join              Descriptor of the current query
    have_min          true if the query selects a MIN function
    have_max          true if the query selects a MAX function
    min_max_arg_part  The only argument field of all MIN/MAX functions
    group_prefix_len  Length of all key parts in the group prefix
    prefix_key_parts  All key parts in the group prefix
    index_info        The index chosen for data access
    use_index         The id of index_info
    read_cost         Cost of this access method
    records           Number of records returned
    key_infix_len     Length of the key infix appended to the group prefix
    key_infix         Infix of constants from equality predicates
    parent_alloc      Memory pool for this and quick_prefix_select data

  RETURN
    None
*/
optimizer::QUICK_GROUP_MIN_MAX_SELECT::
QUICK_GROUP_MIN_MAX_SELECT(Table *table,
                           JOIN *join_arg,
                           bool have_min_arg,
                           bool have_max_arg,
                           KEY_PART_INFO *min_max_arg_part_arg,
                           uint32_t group_prefix_len_arg,
                           uint32_t group_key_parts_arg,
                           uint32_t used_key_parts_arg,
                           KEY *index_info_arg,
                           uint32_t use_index,
                           double read_cost_arg,
                           ha_rows records_arg,
                           uint32_t key_infix_len_arg,
                           unsigned char *key_infix_arg,
                           MEM_ROOT *parent_alloc)
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
  tmp_record= head->record[1];
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
    init_sql_alloc(&alloc, join->session->variables.range_alloc_block_size, 0);
    join->session->mem_root= &alloc;
  }
  else
    memset(&alloc, 0, sizeof(MEM_ROOT));  // ensure that it's not used
}


/*
  Do post-constructor initialization.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::init()

  DESCRIPTION
    The method performs initialization that cannot be done in the constructor
    such as memory allocations that may fail. It allocates memory for the
    group prefix and inifix buffers, and for the lists of MIN/MAX item to be
    updated during execution.

  RETURN
    0      OK
    other  Error code
*/
int optimizer::QUICK_GROUP_MIN_MAX_SELECT::init()
{
  if (group_prefix) /* Already initialized. */
    return 0;

  if (! (last_prefix= (unsigned char*) alloc_root(&alloc, group_prefix_len)))
      return 1;
  /*
    We may use group_prefix to store keys with all select fields, so allocate
    enough space for it.
  */
  if (! (group_prefix= (unsigned char*) alloc_root(&alloc,
                                                   real_prefix_len + min_max_arg_len)))
    return 1;

  if (key_infix_len > 0)
  {
    /*
      The memory location pointed to by key_infix will be deleted soon, so
      allocate a new buffer and copy the key_infix into it.
    */
    unsigned char *tmp_key_infix= (unsigned char*) alloc_root(&alloc, key_infix_len);
    if (! tmp_key_infix)
      return 1;
    memcpy(tmp_key_infix, this->key_infix, key_infix_len);
    this->key_infix= tmp_key_infix;
  }

  if (min_max_arg_part)
  {
    if (my_init_dynamic_array(&min_max_ranges, sizeof(optimizer::QuickRange*), 16, 16))
      return 1;

    if (have_min)
    {
      if (! (min_functions= new List<Item_sum>))
        return 1;
    }
    else
      min_functions= NULL;
    if (have_max)
    {
      if (! (max_functions= new List<Item_sum>))
        return 1;
    }
    else
      max_functions= NULL;

    Item_sum *min_max_item= NULL;
    Item_sum **func_ptr= join->sum_funcs;
    while ((min_max_item= *(func_ptr++)))
    {
      if (have_min && (min_max_item->sum_func() == Item_sum::MIN_FUNC))
        min_functions->push_back(min_max_item);
      else if (have_max && (min_max_item->sum_func() == Item_sum::MAX_FUNC))
        max_functions->push_back(min_max_item);
    }

    if (have_min)
    {
      if (! (min_functions_it= new List_iterator<Item_sum>(*min_functions)))
        return 1;
    }

    if (have_max)
    {
      if (! (max_functions_it= new List_iterator<Item_sum>(*max_functions)))
        return 1;
    }
  }
  else
    min_max_ranges.elements= 0;

  return 0;
}


optimizer::QUICK_GROUP_MIN_MAX_SELECT::~QUICK_GROUP_MIN_MAX_SELECT()
{
  if (cursor->inited != Cursor::NONE)
  {
    cursor->ha_index_end();
  }
  if (min_max_arg_part)
  {
    delete_dynamic(&min_max_ranges);
  }
  free_root(&alloc,MYF(0));
  delete min_functions_it;
  delete max_functions_it;
  delete quick_prefix_select;
}


/*
  Eventually create and add a new quick range object.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::add_range()
    sel_range  Range object from which a

  NOTES
    Construct a new QuickRange object from a SEL_ARG object, and
    add it to the array min_max_ranges. If sel_arg is an infinite
    range, e.g. (x < 5 or x > 4), then skip it and do not construct
    a quick range.

  RETURN
    false on success
    true  otherwise
*/
bool optimizer::QUICK_GROUP_MIN_MAX_SELECT::add_range(SEL_ARG *sel_range)
{
  optimizer::QuickRange *range= NULL;
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
  range= new optimizer::QuickRange(sel_range->min_value,
                                   min_max_arg_len,
                                   make_keypart_map(sel_range->part),
                                   sel_range->max_value,
                                   min_max_arg_len,
                                   make_keypart_map(sel_range->part),
                                   range_flag);
  if (! range)
    return true;
  if (insert_dynamic(&min_max_ranges, (unsigned char*)&range))
    return true;
  return false;
}


/*
  Opens the ranges if there are more conditions in quick_prefix_select than
  the ones used for jumping through the prefixes.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::adjust_prefix_ranges()

  NOTES
    quick_prefix_select is made over the conditions on the whole key.
    It defines a number of ranges of length x.
    However when jumping through the prefixes we use only the the first
    few most significant keyparts in the range key. However if there
    are more keyparts to follow the ones we are using we must make the
    condition on the key inclusive (because x < "ab" means
    x[0] < 'a' OR (x[0] == 'a' AND x[1] < 'b').
    To achive the above we must turn off the NEAR_MIN/NEAR_MAX
*/
void optimizer::QUICK_GROUP_MIN_MAX_SELECT::adjust_prefix_ranges()
{
  if (quick_prefix_select &&
      group_prefix_len < quick_prefix_select->max_used_key_length)
  {
    DYNAMIC_ARRAY *arr= NULL;
    uint32_t inx;

    for (inx= 0, arr= &quick_prefix_select->ranges; inx < arr->elements; inx++)
    {
      optimizer::QuickRange *range= NULL;

      get_dynamic(arr, (unsigned char*)&range, inx);
      range->flag &= ~(NEAR_MIN | NEAR_MAX);
    }
  }
}


/*
  Determine the total number and length of the keys that will be used for
  index lookup.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::update_key_stat()

  DESCRIPTION
    The total length of the keys used for index lookup depends on whether
    there are any predicates referencing the min/max argument, and/or if
    the min/max argument field can be NULL.
    This function does an optimistic analysis whether the search key might
    be extended by a constant for the min/max keypart. It is 'optimistic'
    because during actual execution it may happen that a particular range
    is skipped, and then a shorter key will be used. However this is data
    dependent and can't be easily estimated here.

  RETURN
    None
*/
void optimizer::QUICK_GROUP_MIN_MAX_SELECT::update_key_stat()
{
  max_used_key_length= real_prefix_len;
  if (min_max_ranges.elements > 0)
  {
    optimizer::QuickRange *cur_range= NULL;
    if (have_min)
    { /* Check if the right-most range has a lower boundary. */
      get_dynamic(&min_max_ranges,
                  (unsigned char*) &cur_range,
                  min_max_ranges.elements - 1);
      if (! (cur_range->flag & NO_MIN_RANGE))
      {
        max_used_key_length+= min_max_arg_len;
        used_key_parts++;
        return;
      }
    }
    if (have_max)
    { /* Check if the left-most range has an upper boundary. */
      get_dynamic(&min_max_ranges, (unsigned char*)&cur_range, 0);
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


/*
  Initialize a quick group min/max select for key retrieval.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::reset()

  DESCRIPTION
    Initialize the index chosen for access and find and store the prefix
    of the last group. The method is expensive since it performs disk access.

  RETURN
    0      OK
    other  Error code
*/
int optimizer::QUICK_GROUP_MIN_MAX_SELECT::reset(void)
{
  int result;

  cursor->extra(HA_EXTRA_KEYREAD); /* We need only the key attributes */
  if ((result= cursor->ha_index_init(index,1)))
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



/*
  Get the next key containing the MIN and/or MAX key for the next group.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::get_next()

  DESCRIPTION
    The method finds the next subsequent group of records that satisfies the
    query conditions and finds the keys that contain the MIN/MAX values for
    the key part referenced by the MIN/MAX function(s). Once a group and its
    MIN/MAX values are found, store these values in the Item_sum objects for
    the MIN/MAX functions. The rest of the values in the result row are stored
    in the Item_field::result_field of each select field. If the query does
    not contain MIN and/or MAX functions, then the function only finds the
    group prefix, which is a query answer itself.

  NOTES
    If both MIN and MAX are computed, then we use the fact that if there is
    no MIN key, there can't be a MAX key as well, so we can skip looking
    for a MAX key in this case.

  RETURN
    0                  on success
    HA_ERR_END_OF_FILE if returned all keys
    other              if some error occurred
*/
int optimizer::QUICK_GROUP_MIN_MAX_SELECT::get_next()
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


/*
  Retrieve the minimal key in the next group.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::next_min()

  DESCRIPTION
    Find the minimal key within this group such that the key satisfies the query
    conditions and NULL semantics. The found key is loaded into this->record.

  IMPLEMENTATION
    Depending on the values of min_max_ranges.elements, key_infix_len, and
    whether there is a  NULL in the MIN field, this function may directly
    return without any data access. In this case we use the key loaded into
    this->record by the call to this->next_prefix() just before this call.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if no MIN key was found that fulfills all conditions.
    HA_ERR_END_OF_FILE   - "" -
    other                if some error occurred
*/
int optimizer::QUICK_GROUP_MIN_MAX_SELECT::next_min()
{
  int result= 0;

  /* Find the MIN key using the eventually extended group prefix. */
  if (min_max_ranges.elements > 0)
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


/*
  Retrieve the maximal key in the next group.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::next_max()

  DESCRIPTION
    Lookup the maximal key of the group, and store it into this->record.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if no MAX key was found that fulfills all conditions.
    HA_ERR_END_OF_FILE	 - "" -
    other                if some error occurred
*/
int optimizer::QUICK_GROUP_MIN_MAX_SELECT::next_max()
{
  int result= 0;

  /* Get the last key in the (possibly extended) group. */
  if (min_max_ranges.elements > 0)
    result= next_max_in_range();
  else
    result= cursor->index_read_map(record,
                                   group_prefix,
                                   make_prev_keypart_map(real_key_parts),
                                   HA_READ_PREFIX_LAST);
  return result;
}


/*
  Determine the prefix of the next group.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::next_prefix()

  DESCRIPTION
    Determine the prefix of the next group that satisfies the query conditions.
    If there is a range condition referencing the group attributes, use a
    QuickRangeSelect object to retrieve the *first* key that satisfies the
    condition. If there is a key infix of constants, append this infix
    immediately after the group attributes. The possibly extended prefix is
    stored in this->group_prefix. The first key of the found group is stored in
    this->record, on which relies this->next_min().

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if there is no key with the formed prefix
    HA_ERR_END_OF_FILE   if there are no more keys
    other                if some error occurred
*/
int optimizer::QUICK_GROUP_MIN_MAX_SELECT::next_prefix()
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


/*
  Find the minimal key in a group that satisfies some range conditions for the
  min/max argument field.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::next_min_in_range()

  DESCRIPTION
    Given the sequence of ranges min_max_ranges, find the minimal key that is
    in the left-most possible range. If there is no such key, then the current
    group does not have a MIN key that satisfies the WHERE clause. If a key is
    found, its value is stored in this->record.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if there is no key with the given prefix in any of
                         the ranges
    HA_ERR_END_OF_FILE   - "" -
    other                if some error
*/
int optimizer::QUICK_GROUP_MIN_MAX_SELECT::next_min_in_range()
{
  ha_rkey_function find_flag;
  key_part_map keypart_map;
  optimizer::QuickRange *cur_range= NULL;
  bool found_null= false;
  int result= HA_ERR_KEY_NOT_FOUND;
  basic_string<unsigned char> max_key;

  max_key.reserve(real_prefix_len + min_max_arg_len);

  assert(min_max_ranges.elements > 0);

  for (uint32_t range_idx= 0; range_idx < min_max_ranges.elements; range_idx++)
  { /* Search from the left-most range to the right. */
    get_dynamic(&min_max_ranges, (unsigned char*)&cur_range, range_idx);

    /*
      If the current value for the min/max argument is bigger than the right
      boundary of cur_range, there is no need to check this range.
    */
    if (range_idx != 0 && !(cur_range->flag & NO_MAX_RANGE) &&
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
      memcpy(tmp_record, record, head->s->rec_buff_length);
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
    memcpy(record, tmp_record, head->s->rec_buff_length);
    result= 0;
  }
  return result;
}


/*
  Find the maximal key in a group that satisfies some range conditions for the
  min/max argument field.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::next_max_in_range()

  DESCRIPTION
    Given the sequence of ranges min_max_ranges, find the maximal key that is
    in the right-most possible range. If there is no such key, then the current
    group does not have a MAX key that satisfies the WHERE clause. If a key is
    found, its value is stored in this->record.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if there is no key with the given prefix in any of
                         the ranges
    HA_ERR_END_OF_FILE   - "" -
    other                if some error
*/
int optimizer::QUICK_GROUP_MIN_MAX_SELECT::next_max_in_range()
{
  ha_rkey_function find_flag;
  key_part_map keypart_map;
  optimizer::QuickRange *cur_range= NULL;
  int result= 0;
  basic_string<unsigned char> min_key;
  min_key.reserve(real_prefix_len + min_max_arg_len);

  assert(min_max_ranges.elements > 0);

  for (uint32_t range_idx= min_max_ranges.elements; range_idx > 0; range_idx--)
  { /* Search from the right-most range to the left. */
    get_dynamic(&min_max_ranges, (unsigned char*)&cur_range, range_idx - 1);

    /*
      If the current value for the min/max argument is smaller than the left
      boundary of cur_range, there is no need to check this range.
    */
    if (range_idx != min_max_ranges.elements &&
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


/*
  Update all MIN function results with the newly found value.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::update_min_result()

  DESCRIPTION
    The method iterates through all MIN functions and updates the result value
    of each function by calling Item_sum::reset(), which in turn picks the new
    result value from this->head->record[0], previously updated by
    next_min(). The updated value is stored in a member variable of each of the
    Item_sum objects, depending on the value type.

  IMPLEMENTATION
    The update must be done separately for MIN and MAX, immediately after
    next_min() was called and before next_max() is called, because both MIN and
    MAX take their result value from the same buffer this->head->record[0]
    (i.e.  this->record).

  RETURN
    None
*/
void optimizer::QUICK_GROUP_MIN_MAX_SELECT::update_min_result()
{
  Item_sum *min_func= NULL;

  min_functions_it->rewind();
  while ((min_func= (*min_functions_it)++))
    min_func->reset();
}


/*
  Update all MAX function results with the newly found value.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::update_max_result()

  DESCRIPTION
    The method iterates through all MAX functions and updates the result value
    of each function by calling Item_sum::reset(), which in turn picks the new
    result value from this->head->record[0], previously updated by
    next_max(). The updated value is stored in a member variable of each of the
    Item_sum objects, depending on the value type.

  IMPLEMENTATION
    The update must be done separately for MIN and MAX, immediately after
    next_max() was called, because both MIN and MAX take their result value
    from the same buffer this->head->record[0] (i.e.  this->record).

  RETURN
    None
*/
void optimizer::QUICK_GROUP_MIN_MAX_SELECT::update_max_result()
{
  Item_sum *max_func= NULL;

  max_functions_it->rewind();
  while ((max_func= (*max_functions_it)++))
    max_func->reset();
}


/*
  Append comma-separated list of keys this quick select uses to key_names;
  append comma-separated list of corresponding used lengths to used_lengths.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::add_keys_and_lengths()
    key_names    [out] Names of used indexes
    used_lengths [out] Corresponding lengths of the index names

  DESCRIPTION
    This method is used by select_describe to extract the names of the
    indexes used by a quick select.

*/
void optimizer::QUICK_GROUP_MIN_MAX_SELECT::add_keys_and_lengths(String *key_names,
                                                                 String *used_lengths)
{
  char buf[64];
  uint32_t length;
  key_names->append(index_info->name);
  length= int64_t2str(max_used_key_length, buf, 10) - buf;
  used_lengths->append(buf, length);
}

