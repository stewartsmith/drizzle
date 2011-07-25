/* Copyright (C) 2000-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


/* Some general useful functions */

#include <config.h>

#include <float.h>
#include <fcntl.h>

#include <string>
#include <vector>
#include <algorithm>

#include <drizzled/error.h>
#include <drizzled/gettext.h>

#include <drizzled/plugin/transactional_storage_engine.h>
#include <drizzled/plugin/authorization.h>
#include <drizzled/nested_join.h>
#include <drizzled/sql_parse.h>
#include <drizzled/item/sum.h>
#include <drizzled/table_list.h>
#include <drizzled/session.h>
#include <drizzled/sql_base.h>
#include <drizzled/sql_select.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/varstring.h>
#include <drizzled/field/double.h>
#include <drizzled/message/table.pb.h>
#include <drizzled/sql_table.h>
#include <drizzled/charset.h>
#include <drizzled/internal/m_string.h>
#include <plugin/myisam/myisam.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/item/string.h>
#include <drizzled/item/int.h>
#include <drizzled/item/decimal.h>
#include <drizzled/item/float.h>
#include <drizzled/item/null.h>
#include <drizzled/temporal.h>
#include <drizzled/table/singular.h>
#include <drizzled/table_proto.h>
#include <drizzled/typelib.h>
#include <drizzled/sql_lex.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/system_variables.h>
#include <drizzled/open_tables_state.h>

using namespace std;

namespace drizzled {

extern plugin::StorageEngine *heap_engine;
extern plugin::StorageEngine *myisam_engine;

/* Functions defined in this cursor */

/*************************************************************************/

// @note this should all be the destructor
int Table::delete_table(bool free_share)
{
  int error= 0;

  if (db_stat)
    error= cursor->close();
  _alias.clear();

  if (field)
  {
    for (Field **ptr=field ; *ptr ; ptr++)
    {
      delete *ptr;
    }
    field= 0;
  }
  safe_delete(cursor);

  if (free_share)
  {
    release();
  }

  return error;
}

Table::~Table()
{
  mem_root.free_root(MYF(0));
}


void Table::resetTable(Session *session,
                       TableShare *share,
                       uint32_t db_stat_arg)
{
  setShare(share);
  in_use= session;

  field= NULL;

  cursor= NULL;
  next= NULL;
  prev= NULL;

  read_set= NULL;
  write_set= NULL;

  tablenr= 0;
  db_stat= db_stat_arg;

  record[0]= (unsigned char *) NULL;
  record[1]= (unsigned char *) NULL;

  insert_values.clear();
  key_info= NULL;
  next_number_field= NULL;
  found_next_number_field= NULL;
  timestamp_field= NULL;

  pos_in_table_list= NULL;
  group= NULL;
  _alias.clear();
  null_flags= NULL;

  lock_position= 0;
  lock_data_start= 0;
  lock_count= 0;
  used_fields= 0;
  status= 0;
  derived_select_number= 0;
  current_lock= F_UNLCK;
  copy_blobs= false;

  maybe_null= false;

  null_row= false;

  force_index= false;
  distinct= false;
  const_table= false;
  no_rows= false;
  key_read= false;
  no_keyread= false;

  open_placeholder= false;
  locked_by_name= false;
  no_cache= false;

  auto_increment_field_not_null= false;
  alias_name_used= false;

  query_id= 0;
  quick_condition_rows= 0;

  timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
  map= 0;

  reginfo.reset();

  covering_keys.reset();

  quick_keys.reset();
  merge_keys.reset();

  keys_in_use_for_query.reset();
  keys_in_use_for_group_by.reset();
  keys_in_use_for_order_by.reset();

  memset(quick_rows, 0, sizeof(ha_rows) * MAX_KEY);
  memset(const_key_parts, 0, sizeof(ha_rows) * MAX_KEY);

  memset(quick_key_parts, 0, sizeof(unsigned int) * MAX_KEY);
  memset(quick_n_ranges, 0, sizeof(unsigned int) * MAX_KEY);

  mem_root.init(TABLE_ALLOC_BLOCK_SIZE);
}



/* Deallocate temporary blob storage */

void free_blobs(Table *table)
{
  uint32_t *ptr, *end;
  for (ptr= table->getBlobField(), end=ptr + table->sizeBlobFields();
       ptr != end ;
       ptr++)
  {
    ((Field_blob*) table->getField(*ptr))->free();
  }
}


TYPELIB *typelib(memory::Root& mem_root, List<String> &strings)
{
  TYPELIB *result= new (mem_root) TYPELIB;
  result->count= strings.size();
  result->name= "";
  result->type_names= (const char**) mem_root.alloc((sizeof(char*) + sizeof(uint32_t)) * (result->count + 1));
  result->type_lengths= (uint*) (result->type_names + result->count + 1);

  List<String>::iterator it(strings.begin());
  String *tmp;
  for (uint32_t i= 0; (tmp= it++); i++)
  {
    result->type_names[i]= tmp->ptr();
    result->type_lengths[i]= tmp->length();
  }

  result->type_names[result->count]= 0;   // End marker
  result->type_lengths[result->count]= 0;

  return result;
}

	/* Check that the integer is in the internal */

int set_zone(int nr, int min_zone, int max_zone)
{
  if (nr<=min_zone)
    return (min_zone);
  if (nr>=max_zone)
    return (max_zone);
  return (nr);
} /* set_zone */


/*
  Store an SQL quoted string.

  SYNOPSIS
    append_unescaped()
    res		result String
    pos		string to be quoted
    length	it's length

  NOTE
    This function works correctly with utf8 or single-byte charset strings.
    May fail with some multibyte charsets though.
*/

void append_unescaped(String *res, const char *pos, uint32_t length)
{
  const char *end= pos+length;
  res->append('\'');

  for (; pos != end ; pos++)
  {
    uint32_t mblen;
    if (use_mb(default_charset_info) &&
        (mblen= my_ismbchar(default_charset_info, pos, end)))
    {
      res->append(pos, mblen);
      pos+= mblen - 1;
      if (pos >= end)
        break;
      continue;
    }

    switch (*pos) {
    case 0:				/* Must be escaped for 'mysql' */
      res->append('\\');
      res->append('0');
      break;
    case '\n':				/* Must be escaped for logs */
      res->append('\\');
      res->append('n');
      break;
    case '\r':
      res->append('\\');		/* This gives better readability */
      res->append('r');
      break;
    case '\\':
      res->append('\\');		/* Because of the sql syntax */
      res->append('\\');
      break;
    case '\'':
      res->append('\'');		/* Because of the sql syntax */
      res->append('\'');
      break;
    default:
      res->append(*pos);
      break;
    }
  }
  res->append('\'');
}

/*
  Allow anything as a table name, as long as it doesn't contain an
  ' ' at the end
  returns 1 on error
*/
bool check_table_name(const char *name, uint32_t length)
{
  if (!length || length > NAME_LEN || name[length - 1] == ' ')
    return 1;
  LEX_STRING ident;
  ident.str= (char*) name;
  ident.length= length;
  return check_identifier_name(&ident);
}


/*
  Eventually, a "length" argument should be added
  to this function, and the inner loop changed to
  check_identifier_name() call.
*/
bool check_column_name(const char *name)
{
  uint32_t name_length= 0;  // name length in symbols
  bool last_char_is_space= true;

  while (*name)
  {
    last_char_is_space= my_isspace(system_charset_info, *name);
    if (use_mb(system_charset_info))
    {
      int len=my_ismbchar(system_charset_info, name,
                          name+system_charset_info->mbmaxlen);
      if (len)
      {
        if (len > 3) /* Disallow non-BMP characters */
          return 1;
        name += len;
        name_length++;
        continue;
      }
    }
    /*
      NAMES_SEP_CHAR is used in FRM format to separate SET and ENUM values.
      It is defined as 0xFF, which is a not valid byte in utf8.
      This assert is to catch use of this byte if we decide to
      use non-utf8 as system_character_set.
    */
    assert(*name != NAMES_SEP_CHAR);
    name++;
    name_length++;
  }
  /* Error if empty or too long column name */
  return last_char_is_space || (uint32_t) name_length > NAME_CHAR_LEN;
}


/*****************************************************************************
  Functions to handle column usage bitmaps (read_set, write_set etc...)
*****************************************************************************/

/* Reset all columns bitmaps */

void Table::clear_column_bitmaps()
{
  /*
    Reset column read/write usage. It's identical to:
    bitmap_clear_all(&table->def_read_set);
    bitmap_clear_all(&table->def_write_set);
  */
  def_read_set.reset();
  def_write_set.reset();
  column_bitmaps_set(def_read_set, def_write_set);
}


/*
  Tell Cursor we are going to call position() and rnd_pos() later.

  NOTES:
  This is needed for handlers that uses the primary key to find the
  row. In this case we have to extend the read bitmap with the primary
  key fields.
*/

void Table::prepare_for_position()
{

  if ((cursor->getEngine()->check_flag(HTON_BIT_PRIMARY_KEY_IN_READ_INDEX)) &&
      getShare()->hasPrimaryKey())
  {
    mark_columns_used_by_index_no_reset(getShare()->getPrimaryKey());
  }
  return;
}


/*
  Mark that only fields from one key is used

  NOTE:
    This changes the bitmap to use the tmp bitmap
    After this, you can't access any other columns in the table until
    bitmaps are reset, for example with Table::clear_column_bitmaps()
    or Table::restore_column_maps_after_mark_index()
*/

void Table::mark_columns_used_by_index(uint32_t index)
{
  boost::dynamic_bitset<> *bitmap= &tmp_set;

  (void) cursor->extra(HA_EXTRA_KEYREAD);
  bitmap->reset();
  mark_columns_used_by_index_no_reset(index, *bitmap);
  column_bitmaps_set(*bitmap, *bitmap);
  return;
}


/*
  Restore to use normal column maps after key read

  NOTES
    This reverse the change done by mark_columns_used_by_index

  WARNING
    For this to work, one must have the normal table maps in place
    when calling mark_columns_used_by_index
*/

void Table::restore_column_maps_after_mark_index()
{

  key_read= 0;
  (void) cursor->extra(HA_EXTRA_NO_KEYREAD);
  default_column_bitmaps();
  return;
}


/*
  mark columns used by key, but don't reset other fields
*/

void Table::mark_columns_used_by_index_no_reset(uint32_t index)
{
    mark_columns_used_by_index_no_reset(index, *read_set);
}


void Table::mark_columns_used_by_index_no_reset(uint32_t index,
                                                boost::dynamic_bitset<>& bitmap)
{
  KeyPartInfo *key_part= key_info[index].key_part;
  KeyPartInfo *key_part_end= (key_part + key_info[index].key_parts);
  for (; key_part != key_part_end; key_part++)
  {
    if (! bitmap.empty())
      bitmap.set(key_part->fieldnr-1);
  }
}


/*
  Mark auto-increment fields as used fields in both read and write maps

  NOTES
    This is needed in insert & update as the auto-increment field is
    always set and sometimes read.
*/

void Table::mark_auto_increment_column()
{
  assert(found_next_number_field);
  /*
    We must set bit in read set as update_auto_increment() is using the
    store() to check overflow of auto_increment values
  */
  setReadSet(found_next_number_field->position());
  setWriteSet(found_next_number_field->position());
  if (getShare()->next_number_keypart)
    mark_columns_used_by_index_no_reset(getShare()->next_number_index);
}


/*
  Mark columns needed for doing an delete of a row

  DESCRIPTON
    Some table engines don't have a cursor on the retrieve rows
    so they need either to use the primary key or all columns to
    be able to delete a row.

    If the engine needs this, the function works as follows:
    - If primary key exits, mark the primary key columns to be read.
    - If not, mark all columns to be read

    If the engine has HA_REQUIRES_KEY_COLUMNS_FOR_DELETE, we will
    mark all key columns as 'to-be-read'. This allows the engine to
    loop over the given record to find all keys and doesn't have to
    retrieve the row again.
*/

void Table::mark_columns_needed_for_delete()
{
  /*
    If the Cursor has no cursor capabilites, or we have row-based
    replication active for the current statement, we have to read
    either the primary key, the hidden primary key or all columns to
    be able to do an delete

  */
  if (not getShare()->hasPrimaryKey())
  {
    /* fallback to use all columns in the table to identify row */
    use_all_columns();
    return;
  }
  else
    mark_columns_used_by_index_no_reset(getShare()->getPrimaryKey());

  /* If we the engine wants all predicates we mark all keys */
  if (cursor->getEngine()->check_flag(HTON_BIT_REQUIRES_KEY_COLUMNS_FOR_DELETE))
  {
    Field **reg_field;
    for (reg_field= field ; *reg_field ; reg_field++)
    {
      if ((*reg_field)->flags & PART_KEY_FLAG)
        setReadSet((*reg_field)->position());
    }
  }
}


/*
  Mark columns needed for doing an update of a row

  DESCRIPTON
    Some engines needs to have all columns in an update (to be able to
    build a complete row). If this is the case, we mark all not
    updated columns to be read.

    If this is no the case, we do like in the delete case and mark
    if neeed, either the primary key column or all columns to be read.
    (see mark_columns_needed_for_delete() for details)

    If the engine has HTON_BIT_REQUIRES_KEY_COLUMNS_FOR_DELETE, we will
    mark all USED key columns as 'to-be-read'. This allows the engine to
    loop over the given record to find all changed keys and doesn't have to
    retrieve the row again.
*/

void Table::mark_columns_needed_for_update()
{
  /*
    If the Cursor has no cursor capabilites, or we have row-based
    logging active for the current statement, we have to read either
    the primary key, the hidden primary key or all columns to be
    able to do an update
  */
  if (not getShare()->hasPrimaryKey())
  {
    /* fallback to use all columns in the table to identify row */
    use_all_columns();
    return;
  }
  else
    mark_columns_used_by_index_no_reset(getShare()->getPrimaryKey());

  if (cursor->getEngine()->check_flag(HTON_BIT_REQUIRES_KEY_COLUMNS_FOR_DELETE))
  {
    /* Mark all used key columns for read */
    Field **reg_field;
    for (reg_field= field ; *reg_field ; reg_field++)
    {
      /* Merge keys is all keys that had a column refered to in the query */
      if (is_overlapping(merge_keys, (*reg_field)->part_of_key))
        setReadSet((*reg_field)->position());
    }
  }

}


/*
  Mark columns the Cursor needs for doing an insert

  For now, this is used to mark fields used by the trigger
  as changed.
*/

void Table::mark_columns_needed_for_insert()
{
  if (found_next_number_field)
    mark_auto_increment_column();
}



size_t Table::max_row_length(const unsigned char *data)
{
  size_t length= getRecordLength() + 2 * sizeFields();
  uint32_t *const beg= getBlobField();
  uint32_t *const end= beg + sizeBlobFields();

  for (uint32_t *ptr= beg ; ptr != end ; ++ptr)
  {
    Field_blob* const blob= (Field_blob*) field[*ptr];
    length+= blob->get_length((const unsigned char*)
                              (data + blob->offset(getInsertRecord()))) +
      HA_KEY_BLOB_LENGTH;
  }
  return length;
}

void Table::setVariableWidth(void)
{
  assert(in_use);
  if (in_use && in_use->lex().sql_command == SQLCOM_CREATE_TABLE)
  {
    getMutableShare()->setVariableWidth();
    return;
  }

  assert(0); // Programming error, you can't set this on a plain old Table.
}

/****************************************************************************
 Functions for creating temporary tables.
****************************************************************************/
/**
  Create field for temporary table from given field.

  @param session	       Thread Cursor
  @param org_field    field from which new field will be created
  @param name         New field name
  @param table	       Temporary table
  @param item	       !=NULL if item->result_field should point to new field.
                      This is relevant for how fill_record() is going to work:
                      If item != NULL then fill_record() will update
                      the record in the original table.
                      If item == NULL then fill_record() will update
                      the temporary table
  @param convert_blob_length   If >0 create a varstring(convert_blob_length)
                               field instead of blob.

  @retval
    NULL		on error
  @retval
    new_created field
*/

Field *create_tmp_field_from_field(Session *session, Field *org_field,
                                   const char *name, Table *table,
                                   Item_field *item, uint32_t convert_blob_length)
{
  Field *new_field;

  /*
    Make sure that the blob fits into a Field_varstring which has
    2-byte lenght.
  */
  if (convert_blob_length && convert_blob_length <= Field_varstring::MAX_SIZE && (org_field->flags & BLOB_FLAG))
  {
    table->setVariableWidth();
    new_field= new Field_varstring(convert_blob_length, org_field->maybe_null(), org_field->field_name, org_field->charset());
  }
  else
  {
    new_field= org_field->new_field(session->mem_root, table, table == org_field->getTable());
  }
  if (new_field)
  {
    new_field->init(table);
    new_field->orig_table= org_field->orig_table;
    if (item)
      item->result_field= new_field;
    else
      new_field->field_name= name;
    new_field->flags|= (org_field->flags & NO_DEFAULT_VALUE_FLAG);
    if (org_field->maybe_null() || (item && item->maybe_null))
      new_field->flags&= ~NOT_NULL_FLAG;	// Because of outer join
    if (org_field->type() == DRIZZLE_TYPE_VARCHAR)
      table->getMutableShare()->db_create_options|= HA_OPTION_PACK_RECORD;
    else if (org_field->type() == DRIZZLE_TYPE_DOUBLE)
      ((Field_double *) new_field)->not_fixed= true;
  }
  return new_field;
}


/**
  Create a temp table according to a field list.

  Given field pointers are changed to point at tmp_table for
  send_fields. The table object is self contained: it's
  allocated in its own memory root, as well as Field objects
  created for table columns.
  This function will replace Item_sum items in 'fields' list with
  corresponding Item_field items, pointing at the fields in the
  temporary table, unless this was prohibited by true
  value of argument save_sum_fields. The Item_field objects
  are created in Session memory root.

  @param session                  thread handle
  @param param                a description used as input to create the table
  @param fields               list of items that will be used to define
                              column types of the table (also see NOTES)
  @param group                TODO document
  @param distinct             should table rows be distinct
  @param save_sum_fields      see NOTES
  @param select_options
  @param rows_limit
  @param table_alias          possible name of the temporary table that can
                              be used for name resolving; can be "".
*/

Table *
create_tmp_table(Session *session,Tmp_Table_Param *param,List<Item> &fields,
		 Order *group, bool distinct, bool save_sum_fields,
		 uint64_t select_options, ha_rows rows_limit,
		 const char *table_alias)
{
  uint	i,field_count,null_count,null_pack_length;
  uint32_t  copy_func_count= param->func_count;
  uint32_t  hidden_null_count, hidden_null_pack_length, hidden_field_count;
  uint32_t  blob_count,group_null_items, string_count;
  uint32_t fieldnr= 0;
  ulong reclength, string_total_length;
  bool  using_unique_constraint= false;
  bool  not_all_columns= !(select_options & TMP_TABLE_ALL_COLUMNS);
  unsigned char	*pos, *group_buff;
  unsigned char *null_flags;
  Field **reg_field, **from_field, **default_field;
  CopyField *copy= 0;
  KeyInfo *keyinfo;
  KeyPartInfo *key_part_info;
  Item **copy_func;
  MI_COLUMNDEF *recinfo;
  uint32_t total_uneven_bit_length= 0;
  bool force_copy_fields= param->force_copy_fields;
  uint64_t max_rows= 0;

  session->status_var.created_tmp_tables++;

  if (group)
  {
    if (! param->quick_group)
    {
      group= 0;					// Can't use group key
    }
    else for (Order *tmp=group ; tmp ; tmp=tmp->next)
    {
      /*
        marker == 4 means two things:
        - store NULLs in the key, and
        - convert BIT fields to 64-bit long, needed because MEMORY tables
          can't index BIT fields.
      */
      (*tmp->item)->marker= 4;
      if ((*tmp->item)->max_length >= CONVERT_IF_BIGGER_TO_BLOB)
	using_unique_constraint= true;
    }
    if (param->group_length >= MAX_BLOB_WIDTH)
      using_unique_constraint= true;
    if (group)
      distinct= 0;				// Can't use distinct
  }

  field_count=param->field_count+param->func_count+param->sum_func_count;
  hidden_field_count=param->hidden_field_count;

  /*
    When loose index scan is employed as access method, it already
    computes all groups and the result of all aggregate functions. We
    make space for the items of the aggregate function in the list of
    functions Tmp_Table_Param::items_to_copy, so that the values of
    these items are stored in the temporary table.
  */
  if (param->precomputed_group_by)
  {
    copy_func_count+= param->sum_func_count;
  }

  table::Singular* table= &session->getInstanceTable(); // This will not go into the tableshare cache, so no key is used.

  table->mem().multi_alloc(0,
    &default_field, sizeof(Field*) * (field_count),
    &from_field, sizeof(Field*)*field_count,
    &copy_func, sizeof(*copy_func)*(copy_func_count+1),
    &param->keyinfo, sizeof(*param->keyinfo),
    &key_part_info, sizeof(*key_part_info)*(param->group_parts+1),
    &param->start_recinfo, sizeof(*param->recinfo)*(field_count*2+4),
    &group_buff, (group && ! using_unique_constraint ? param->group_length : 0),
    NULL);
  /* CopyField belongs to Tmp_Table_Param, allocate it in Session mem_root */
  param->copy_field= copy= new (session->mem_root) CopyField[field_count];
  param->items_to_copy= copy_func;
  /* make table according to fields */

  memset(default_field, 0, sizeof(Field*) * (field_count));
  memset(from_field, 0, sizeof(Field*)*field_count);

  memory::Root* mem_root_save= session->mem_root;
  session->mem_root= &table->mem();

  table->getMutableShare()->setFields(field_count+1);
  table->setFields(table->getMutableShare()->getFields(true));
  reg_field= table->getMutableShare()->getFields(true);
  table->setAlias(table_alias);
  table->reginfo.lock_type=TL_WRITE;	/* Will be updated */
  table->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  table->map=1;
  table->copy_blobs= 1;
  assert(session);
  table->in_use= session;
  table->quick_keys.reset();
  table->covering_keys.reset();
  table->keys_in_use_for_query.reset();

  table->getMutableShare()->blob_field.resize(field_count+1);
  uint32_t *blob_field= &table->getMutableShare()->blob_field[0];
  table->getMutableShare()->db_low_byte_first=1;                // True for HEAP and MyISAM
  table->getMutableShare()->table_charset= param->table_charset;
  table->getMutableShare()->keys_for_keyread.reset();
  table->getMutableShare()->keys_in_use.reset();

  /* Calculate which type of fields we will store in the temporary table */

  reclength= string_total_length= 0;
  blob_count= string_count= null_count= hidden_null_count= group_null_items= 0;
  param->using_indirect_summary_function= 0;

  List<Item>::iterator li(fields.begin());
  Field **tmp_from_field=from_field;
  while (Item* item=li++)
  {
    Item::Type type=item->type();
    if (not_all_columns)
    {
      if (item->with_sum_func && type != Item::SUM_FUNC_ITEM)
      {
        if (item->used_tables() & OUTER_REF_TABLE_BIT)
          item->update_used_tables();
        if (type == Item::SUBSELECT_ITEM ||
            (item->used_tables() & ~OUTER_REF_TABLE_BIT))
        {
	  /*
	    Mark that the we have ignored an item that refers to a summary
	    function. We need to know this if someone is going to use
	    DISTINCT on the result.
	  */
	  param->using_indirect_summary_function=1;
	  continue;
        }
      }
      if (item->const_item() && (int) hidden_field_count <= 0)
        continue; // We don't have to store this
    }
    if (type == Item::SUM_FUNC_ITEM && !group && !save_sum_fields)
    {						/* Can't calc group yet */
      ((Item_sum*) item)->result_field= 0;
      for (i= 0 ; i < ((Item_sum*) item)->arg_count ; i++)
      {
	Item **argp= ((Item_sum*) item)->args + i;
	Item *arg= *argp;
	if (!arg->const_item())
	{
	  Field *new_field=
            create_tmp_field(session, table, arg, arg->type(), &copy_func,
                             tmp_from_field, &default_field[fieldnr],
                             group != 0,not_all_columns,
                             false,
                             param->convert_blob_length);
	  if (!new_field)
	    goto err;					// Should be OOM
	  tmp_from_field++;
	  reclength+=new_field->pack_length();
	  if (new_field->flags & BLOB_FLAG)
	  {
	    *blob_field++= fieldnr;
	    blob_count++;
	  }
	  *(reg_field++)= new_field;
          if (new_field->real_type() == DRIZZLE_TYPE_VARCHAR)
          {
            string_count++;
            string_total_length+= new_field->pack_length();
          }
          session->mem_root= mem_root_save;
          *argp= new Item_field(new_field);
          session->mem_root= &table->mem();
	  if (!(new_field->flags & NOT_NULL_FLAG))
          {
	    null_count++;
            /*
              new_field->maybe_null() is still false, it will be
              changed below. But we have to setup Item_field correctly
            */
            (*argp)->maybe_null=1;
          }
          new_field->setPosition(fieldnr++);
	}
      }
    }
    else
    {
      /*
	The last parameter to create_tmp_field() is a bit tricky:

	We need to set it to 0 in union, to get fill_record() to modify the
	temporary table.
	We need to set it to 1 on multi-table-update and in select to
	write rows to the temporary table.
	We here distinguish between UNION and multi-table-updates by the fact
	that in the later case group is set to the row pointer.
      */
      Field *new_field=
        create_tmp_field(session, table, item, type, &copy_func,
                         tmp_from_field, &default_field[fieldnr],
                         group != 0,
                         !force_copy_fields &&
                           (not_all_columns || group != 0),
                         force_copy_fields,
                         param->convert_blob_length);

      if (!new_field)
      {
	if (session->is_fatal_error)
	  goto err;				// Got OOM
	continue;				// Some kindf of const item
      }
      if (type == Item::SUM_FUNC_ITEM)
	((Item_sum *) item)->result_field= new_field;
      tmp_from_field++;
      reclength+=new_field->pack_length();
      if (!(new_field->flags & NOT_NULL_FLAG))
	null_count++;
      if (new_field->flags & BLOB_FLAG)
      {
        *blob_field++= fieldnr;
	blob_count++;
      }
      if (item->marker == 4 && item->maybe_null)
      {
	group_null_items++;
	new_field->flags|= GROUP_FLAG;
      }
      new_field->setPosition(fieldnr++);
      *(reg_field++)= new_field;
    }
    if (!--hidden_field_count)
    {
      /*
        This was the last hidden field; Remember how many hidden fields could
        have null
      */
      hidden_null_count=null_count;
      /*
	We need to update hidden_field_count as we may have stored group
	functions with constant arguments
      */
      param->hidden_field_count= fieldnr;
      null_count= 0;
    }
  }
  assert(fieldnr == (uint32_t) (reg_field - table->getFields()));
  assert(field_count >= (uint32_t) (reg_field - table->getFields()));
  field_count= fieldnr;
  *reg_field= 0;
  *blob_field= 0;				// End marker
  table->getMutableShare()->setFieldSize(field_count);

  /* If result table is small; use a heap */
  /* future: storage engine selection can be made dynamic? */
  if (blob_count || using_unique_constraint || 
      (session->lex().select_lex.options & SELECT_BIG_RESULT) ||
      (session->lex().current_select->olap == ROLLUP_TYPE) ||
      (select_options & (OPTION_BIG_TABLES | SELECT_SMALL_RESULT)) == OPTION_BIG_TABLES)
  {
    table->getMutableShare()->storage_engine= myisam_engine;
    table->cursor= table->getMutableShare()->db_type()->getCursor(*table);
    if (group &&
	(param->group_parts > table->cursor->getEngine()->max_key_parts() ||
	 param->group_length > table->cursor->getEngine()->max_key_length()))
    {
      using_unique_constraint= true;
    }
  }
  else
  {
    table->getMutableShare()->storage_engine= heap_engine;
    table->cursor= table->getMutableShare()->db_type()->getCursor(*table);
  }
  if (! table->cursor)
    goto err;


  if (! using_unique_constraint)
    reclength+= group_null_items;	// null flag is stored separately

  table->getMutableShare()->blob_fields= blob_count;
  if (blob_count == 0)
  {
    /* We need to ensure that first byte is not 0 for the delete link */
    if (param->hidden_field_count)
      hidden_null_count++;
    else
      null_count++;
  }
  hidden_null_pack_length=(hidden_null_count+7)/8;
  null_pack_length= (hidden_null_pack_length +
                     (null_count + total_uneven_bit_length + 7) / 8);
  reclength+=null_pack_length;
  if (!reclength)
    reclength=1;				// Dummy select

  table->getMutableShare()->setRecordLength(reclength);
  {
    uint32_t alloc_length=ALIGN_SIZE(reclength+MI_UNIQUE_HASH_LENGTH+1);
    table->getMutableShare()->rec_buff_length= alloc_length;
    table->record[0]= table->alloc(alloc_length*2);
    table->record[1]= table->getInsertRecord()+alloc_length;
    table->getMutableShare()->resizeDefaultValues(alloc_length);
  }
  copy_func[0]= 0;				// End marker
  param->func_count= copy_func - param->items_to_copy;

  table->setup_tmp_table_column_bitmaps();

  recinfo=param->start_recinfo;
  null_flags= table->getInsertRecord();
  pos=table->getInsertRecord()+ null_pack_length;
  if (null_pack_length)
  {
    memset(recinfo, 0, sizeof(*recinfo));
    recinfo->type=FIELD_NORMAL;
    recinfo->length=null_pack_length;
    recinfo++;
    memset(null_flags, 255, null_pack_length);	// Set null fields

    table->null_flags= table->getInsertRecord();
    table->getMutableShare()->null_fields= null_count+ hidden_null_count;
    table->getMutableShare()->null_bytes= null_pack_length;
  }
  null_count= (blob_count == 0) ? 1 : 0;
  hidden_field_count=param->hidden_field_count;
  for (i= 0,reg_field= table->getFields(); i < field_count; i++,reg_field++,recinfo++)
  {
    Field *field= *reg_field;
    uint32_t length;
    memset(recinfo, 0, sizeof(*recinfo));

    if (!(field->flags & NOT_NULL_FLAG))
    {
      if (field->flags & GROUP_FLAG && !using_unique_constraint)
      {
	/*
	  We have to reserve one byte here for NULL bits,
	  as this is updated by 'end_update()'
	*/
	*pos++= '\0';				// Null is stored here
	recinfo->length= 1;
	recinfo->type=FIELD_NORMAL;
	recinfo++;
	memset(recinfo, 0, sizeof(*recinfo));
      }
      else
      {
	recinfo->null_bit= 1 << (null_count & 7);
	recinfo->null_pos= null_count/8;
      }
      field->move_field(pos,null_flags+null_count/8,
			1 << (null_count & 7));
      null_count++;
    }
    else
      field->move_field(pos,(unsigned char*) 0,0);
    field->reset();

    /*
      Test if there is a default field value. The test for ->ptr is to skip
      'offset' fields generated by initalize_tables
    */
    if (default_field[i] && default_field[i]->ptr)
    {
      /*
         default_field[i] is set only in the cases  when 'field' can
         inherit the default value that is defined for the field referred
         by the Item_field object from which 'field' has been created.
      */
      ptrdiff_t diff;
      Field *orig_field= default_field[i];
      /* Get the value from default_values */
      diff= (ptrdiff_t) (orig_field->getTable()->getDefaultValues() - orig_field->getTable()->getInsertRecord());
      orig_field->move_field_offset(diff);      // Points now at default_values
      if (orig_field->is_real_null())
        field->set_null();
      else
      {
        field->set_notnull();
        memcpy(field->ptr, orig_field->ptr, field->pack_length());
      }
      orig_field->move_field_offset(-diff);     // Back to getInsertRecord()
    }

    if (from_field[i])
    {						/* Not a table Item */
      copy->set(field,from_field[i],save_sum_fields);
      copy++;
    }
    length=field->pack_length();
    pos+= length;

    /* Make entry for create table */
    recinfo->length=length;
    if (field->flags & BLOB_FLAG)
      recinfo->type= (int) FIELD_BLOB;
    else
      recinfo->type=FIELD_NORMAL;
    if (!--hidden_field_count)
      null_count=(null_count+7) & ~7;		// move to next byte
  }

  param->copy_field_end=copy;
  param->recinfo=recinfo;
  table->storeRecordAsDefault();        // Make empty default record

  if (session->variables.tmp_table_size == ~ (uint64_t) 0)		// No limit
  {
    max_rows= ~(uint64_t) 0;
  }
  else
  {
    max_rows= (uint64_t) (((table->getMutableShare()->db_type() == heap_engine) ?
                           min(session->variables.tmp_table_size,
                               session->variables.max_heap_table_size) :
                           session->variables.tmp_table_size) /
                          table->getMutableShare()->getRecordLength());
  }

  set_if_bigger(max_rows, (uint64_t)1);	// For dummy start options
  /*
    Push the LIMIT clause to the temporary table creation, so that we
    materialize only up to 'rows_limit' records instead of all result records.
  */
  set_if_smaller(max_rows, rows_limit);

  table->getMutableShare()->setMaxRows(max_rows);

  param->end_write_records= rows_limit;

  keyinfo= param->keyinfo;

  if (group)
  {
    table->group=group;				/* Table is grouped by key */
    param->group_buff=group_buff;
    table->getMutableShare()->keys=1;
    table->getMutableShare()->uniques= test(using_unique_constraint);
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME;
    keyinfo->usable_key_parts=keyinfo->key_parts= param->group_parts;
    keyinfo->key_length= 0;
    keyinfo->rec_per_key= 0;
    keyinfo->algorithm= HA_KEY_ALG_UNDEF;
    keyinfo->name= (char*) "group_key";
    Order *cur_group= group;
    for (; cur_group ; cur_group= cur_group->next, key_part_info++)
    {
      Field *field=(*cur_group->item)->get_tmp_table_field();
      bool maybe_null=(*cur_group->item)->maybe_null;
      key_part_info->null_bit= 0;
      key_part_info->field=  field;
      key_part_info->offset= field->offset(table->getInsertRecord());
      key_part_info->length= (uint16_t) field->key_length();
      key_part_info->type=   (uint8_t) field->key_type();
      key_part_info->key_type= 
	((ha_base_keytype) key_part_info->type == HA_KEYTYPE_TEXT ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT1 ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT2) ?
	0 : 1;
      if (!using_unique_constraint)
      {
	cur_group->buff=(char*) group_buff;
	if (!(cur_group->field= field->new_key_field(session->mem_root,table,
                                                     group_buff +
                                                     test(maybe_null),
                                                     field->null_ptr,
                                                     field->null_bit)))
	  goto err;
	if (maybe_null)
	{
	  /*
	    To be able to group on NULL, we reserved place in group_buff
	    for the NULL flag just before the column. (see above).
	    The field data is after this flag.
	    The NULL flag is updated in 'end_update()' and 'end_write()'
	  */
	  keyinfo->flags|= HA_NULL_ARE_EQUAL;	// def. that NULL == NULL
	  key_part_info->null_bit=field->null_bit;
	  key_part_info->null_offset= (uint32_t) (field->null_ptr -
					      (unsigned char*) table->getInsertRecord());
          cur_group->buff++;                        // Pointer to field data
	  group_buff++;                         // Skipp null flag
	}
        /* In GROUP BY 'a' and 'a ' are equal for VARCHAR fields */
        key_part_info->key_part_flag|= HA_END_SPACE_ARE_EQUAL;
	group_buff+= cur_group->field->pack_length();
      }
      keyinfo->key_length+=  key_part_info->length;
    }
  }

  if (distinct && field_count != param->hidden_field_count)
  {
    /*
      Create an unique key or an unique constraint over all columns
      that should be in the result.  In the temporary table, there are
      'param->hidden_field_count' extra columns, whose null bits are stored
      in the first 'hidden_null_pack_length' bytes of the row.
    */
    if (blob_count)
    {
      /*
        Special mode for index creation in MyISAM used to support unique
        indexes on blobs with arbitrary length. Such indexes cannot be
        used for lookups.
      */
      table->getMutableShare()->uniques= 1;
    }
    null_pack_length-=hidden_null_pack_length;
    keyinfo->key_parts= ((field_count-param->hidden_field_count)+
			 (table->getMutableShare()->uniques ? test(null_pack_length) : 0));
    table->distinct= 1;
    table->getMutableShare()->keys= 1;
    key_part_info= new (table->mem()) KeyPartInfo[keyinfo->key_parts];
    memset(key_part_info, 0, keyinfo->key_parts * sizeof(KeyPartInfo));
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME | HA_NULL_ARE_EQUAL;
    keyinfo->key_length=(uint16_t) reclength;
    keyinfo->name= (char*) "distinct_key";
    keyinfo->algorithm= HA_KEY_ALG_UNDEF;
    keyinfo->rec_per_key= 0;

    /*
      Create an extra field to hold NULL bits so that unique indexes on
      blobs can distinguish NULL from 0. This extra field is not needed
      when we do not use UNIQUE indexes for blobs.
    */
    if (null_pack_length && table->getMutableShare()->uniques)
    {
      key_part_info->null_bit= 0;
      key_part_info->offset=hidden_null_pack_length;
      key_part_info->length=null_pack_length;
      table->setVariableWidth();
      key_part_info->field= new Field_varstring(table->getInsertRecord(),
                                                (uint32_t) key_part_info->length,
                                                0,
                                                (unsigned char*) 0,
                                                (uint32_t) 0,
                                                NULL,
                                                &my_charset_bin);
      if (!key_part_info->field)
        goto err;
      key_part_info->field->init(table);
      key_part_info->key_type= 1; /* binary comparison */
      key_part_info->type=    HA_KEYTYPE_BINARY;
      key_part_info++;
    }
    /* Create a distinct key over the columns we are going to return */
    for (i=param->hidden_field_count, reg_field=table->getFields() + i ;
	 i < field_count;
	 i++, reg_field++, key_part_info++)
    {
      key_part_info->null_bit= 0;
      key_part_info->field=    *reg_field;
      key_part_info->offset=   (*reg_field)->offset(table->getInsertRecord());
      key_part_info->length=   (uint16_t) (*reg_field)->pack_length();
      /* @todo The below method of computing the key format length of the
        key part is a copy/paste from optimizer/range.cc, and table.cc.
        This should be factored out, e.g. as a method of Field.
        In addition it is not clear if any of the Field::*_length
        methods is supposed to compute the same length. If so, it
        might be reused.
      */
      key_part_info->store_length= key_part_info->length;

      if ((*reg_field)->real_maybe_null())
        key_part_info->store_length+= HA_KEY_NULL_LENGTH;
      if ((*reg_field)->type() == DRIZZLE_TYPE_BLOB ||
          (*reg_field)->real_type() == DRIZZLE_TYPE_VARCHAR)
        key_part_info->store_length+= HA_KEY_BLOB_LENGTH;

      key_part_info->type=     (uint8_t) (*reg_field)->key_type();
      key_part_info->key_type =
	((ha_base_keytype) key_part_info->type == HA_KEYTYPE_TEXT ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT1 ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT2) ?
	0 : 1;
    }
  }

  if (session->is_fatal_error)				// If end of memory
    goto err;
  table->getMutableShare()->db_record_offset= 1;
  if (table->getShare()->db_type() == myisam_engine)
  {
    if (table->create_myisam_tmp_table(param->keyinfo, param->start_recinfo,
				       &param->recinfo, select_options))
      goto err;
  }
  assert(table->in_use);
  if (table->open_tmp_table())
    goto err;

  session->mem_root= mem_root_save;

  return(table);

err:
  session->mem_root= mem_root_save;
  table= NULL;

  return NULL;
}

/****************************************************************************/

void Table::column_bitmaps_set(boost::dynamic_bitset<>& read_set_arg,
                               boost::dynamic_bitset<>& write_set_arg)
{
  read_set= &read_set_arg;
  write_set= &write_set_arg;
}


const boost::dynamic_bitset<> Table::use_all_columns(boost::dynamic_bitset<>& in_map)
{
  const boost::dynamic_bitset<> old= in_map;
  in_map= getShare()->all_set;
  return old;
}

void Table::restore_column_map(const boost::dynamic_bitset<>& old)
{
  for (boost::dynamic_bitset<>::size_type i= 0; i < old.size(); i++)
  {
    if (old.test(i))
    {
      read_set->set(i);
    }
    else
    {
      read_set->reset(i);
    }
  }
}

uint32_t Table::find_shortest_key(const key_map *usable_keys)
{
  uint32_t min_length= UINT32_MAX;
  uint32_t best= MAX_KEY;
  if (usable_keys->any())
  {
    for (uint32_t nr= 0; nr < getShare()->sizeKeys() ; nr++)
    {
      if (usable_keys->test(nr))
      {
        if (key_info[nr].key_length < min_length)
        {
          min_length= key_info[nr].key_length;
          best=nr;
        }
      }
    }
  }
  return best;
}

/*****************************************************************************
  Remove duplicates from tmp table
  This should be recoded to add a unique index to the table and remove
  duplicates
  Table is a locked single thread table
  fields is the number of fields to check (from the end)
*****************************************************************************/

bool Table::compare_record(Field **ptr)
{
  for (; *ptr ; ptr++)
  {
    if ((*ptr)->cmp_offset(getShare()->rec_buff_length))
      return true;
  }
  return false;
}

/**
   True if the table's input and output record buffers are comparable using
   compare_records(TABLE*).
 */
bool Table::records_are_comparable()
{
  return ((getEngine()->check_flag(HTON_BIT_PARTIAL_COLUMN_READ) == 0) ||
          write_set->is_subset_of(*read_set));
}

/**
   Compares the input and outbut record buffers of the table to see if a row
   has changed. The algorithm iterates over updated columns and if they are
   nullable compares NULL bits in the buffer before comparing actual
   data. Special care must be taken to compare only the relevant NULL bits and
   mask out all others as they may be undefined. The storage engine will not
   and should not touch them.

   @param table The table to evaluate.

   @return true if row has changed.
   @return false otherwise.
*/
bool Table::compare_records()
{
  if (getEngine()->check_flag(HTON_BIT_PARTIAL_COLUMN_READ) != 0)
  {
    /*
      Storage engine may not have read all columns of the record.  Fields
      (including NULL bits) not in the write_set may not have been read and
      can therefore not be compared.
    */
    for (Field **ptr= this->field ; *ptr != NULL; ptr++)
    {
      Field *f= *ptr;
      if (write_set->test(f->position()))
      {
        if (f->real_maybe_null())
        {
          unsigned char null_byte_index= f->null_ptr - record[0];

          if (((record[0][null_byte_index]) & f->null_bit) !=
              ((record[1][null_byte_index]) & f->null_bit))
            return true;
        }
        if (f->cmp_binary_offset(getShare()->rec_buff_length))
          return true;
      }
    }
    return false;
  }

  /*
    The storage engine has read all columns, so it's safe to compare all bits
    including those not in the write_set. This is cheaper than the
    field-by-field comparison done above.
  */
  if (not getShare()->blob_fields + getShare()->hasVariableWidth())
    // Fixed-size record: do bitwise comparison of the records
    return memcmp(this->getInsertRecord(), this->getUpdateRecord(), (size_t) getShare()->getRecordLength());

  /* Compare null bits */
  if (memcmp(null_flags, null_flags + getShare()->rec_buff_length, getShare()->null_bytes))
    return true; /* Diff in NULL value */

  /* Compare updated fields */
  for (Field **ptr= field ; *ptr ; ptr++)
  {
    if (isWriteSet((*ptr)->position()) &&
	(*ptr)->cmp_binary_offset(getShare()->rec_buff_length))
      return true;
  }
  return false;
}

/*
 * Store a record from previous record into next
 *
 */
void Table::storeRecord()
{
  memcpy(getUpdateRecord(), getInsertRecord(), (size_t) getShare()->getRecordLength());
}

/*
 * Store a record as an insert
 *
 */
void Table::storeRecordAsInsert()
{
  assert(insert_values.size() >= getShare()->getRecordLength());
  memcpy(&insert_values[0], getInsertRecord(), (size_t) getShare()->getRecordLength());
}

/*
 * Store a record with default values
 *
 */
void Table::storeRecordAsDefault()
{
  memcpy(getMutableShare()->getDefaultValues(), getInsertRecord(), (size_t) getShare()->getRecordLength());
}

/*
 * Restore a record from previous record into next
 *
 */
void Table::restoreRecord()
{
  memcpy(getInsertRecord(), getUpdateRecord(), (size_t) getShare()->getRecordLength());
}

/*
 * Restore a record with default values
 *
 */
void Table::restoreRecordAsDefault()
{
  memcpy(getInsertRecord(), getMutableShare()->getDefaultValues(), (size_t) getShare()->getRecordLength());
}

/*
 * Empty a record
 *
 */
void Table::emptyRecord()
{
  restoreRecordAsDefault();
  memset(null_flags, 255, getShare()->null_bytes);
}

Table::Table() : 
  field(NULL),
  cursor(NULL),
  next(NULL),
  prev(NULL),
  read_set(NULL),
  write_set(NULL),
  tablenr(0),
  db_stat(0),
  def_read_set(),
  def_write_set(),
  tmp_set(),
  in_use(NULL),
  key_info(NULL),
  next_number_field(NULL),
  found_next_number_field(NULL),
  timestamp_field(NULL),
  pos_in_table_list(NULL),
  group(NULL),
  null_flags(NULL),
  lock_position(0),
  lock_data_start(0),
  lock_count(0),
  used_fields(0),
  status(0),
  derived_select_number(0),
  current_lock(F_UNLCK),
  copy_blobs(false),
  maybe_null(false),
  null_row(false),
  force_index(false),
  distinct(false),
  const_table(false),
  no_rows(false),
  key_read(false),
  no_keyread(false),
  open_placeholder(false),
  locked_by_name(false),
  no_cache(false),
  auto_increment_field_not_null(false),
  alias_name_used(false),
  query_id(0),
  quick_condition_rows(0),
  timestamp_field_type(TIMESTAMP_NO_AUTO_SET),
  map(0),
  quick_rows(),
  const_key_parts(),
  quick_key_parts(),
  quick_n_ranges()
{
  record[0]= (unsigned char *) 0;
  record[1]= (unsigned char *) 0;
}

/*****************************************************************************
  The different ways to read a record
  Returns -1 if row was not found, 0 if row was found and 1 on errors
*****************************************************************************/

/** Help function when we get some an error from the table Cursor. */

int Table::report_error(int error)
{
  if (error == HA_ERR_END_OF_FILE || error == HA_ERR_KEY_NOT_FOUND)
  {
    status= STATUS_GARBAGE;
    return -1;					// key not found; ok
  }
  /*
    Locking reads can legally return also these errors, do not
    print them to the .err log
  */
  if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
    errmsg_printf(error::ERROR, _("Got error %d when reading table '%s'"),
                  error, getShare()->getPath());
  print_error(error, MYF(0));

  return 1;
}


void Table::setup_table_map(TableList *table_list, uint32_t table_number)
{
  used_fields= 0;
  const_table= 0;
  null_row= 0;
  status= STATUS_NO_RECORD;
  maybe_null= table_list->outer_join;
  TableList *embedding= table_list->getEmbedding();
  while (!maybe_null && embedding)
  {
    maybe_null= embedding->outer_join;
    embedding= embedding->getEmbedding();
  }
  tablenr= table_number;
  map= (table_map) 1 << table_number;
  force_index= table_list->force_index;
  covering_keys= getShare()->keys_for_keyread;
  merge_keys.reset();
}


void Table::fill_item_list(List<Item>& items) const
{
  /*
    All Item_field's created using a direct pointer to a field
    are fixed in Item_field constructor.
  */
  for (Field **ptr= field; *ptr; ptr++)
    items.push_back(new Item_field(*ptr));
}


void Table::filesort_free_buffers(bool full)
{
  free(sort.record_pointers);
  sort.record_pointers=0;
  if (full)
  {
    free(sort.sort_keys);
    sort.sort_keys= 0;
    free(sort.buffpek);
    sort.buffpek= 0;
    sort.buffpek_len= 0;
  }
  free(sort.addon_buf);
  free(sort.addon_field);
  sort.addon_buf=0;
  sort.addon_field=0;
}

/*
  Is this instance of the table should be reopen or represents a name-lock?
*/
bool Table::needs_reopen_or_name_lock() const
{ 
  return getShare()->getVersion() != g_refresh_version;
}

uint32_t Table::index_flags(uint32_t idx) const
{
  return getShare()->getEngine()->index_flags(getShare()->getKeyInfo(idx).algorithm);
}

void Table::print_error(int error, myf errflag) const
{
  getShare()->getEngine()->print_error(error, errflag, *this);
}

} /* namespace drizzled */
