/* -*- mode: c++; c-basic-offset: 2; i/dent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/*
  This class is shared between different table objects. There is one
  instance of table share per one table in the database.
*/

/* Basic functions needed by many modules */
#include <config.h>

#include <pthread.h>
#include <float.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <cassert>

#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/sql_base.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/internal/my_pthread.h>

#include <drizzled/table.h>
#include <drizzled/table/shell.h>

#include <drizzled/session.h>

#include <drizzled/charset.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/internal/my_sys.h>

#include <drizzled/item/string.h>
#include <drizzled/item/int.h>
#include <drizzled/item/decimal.h>
#include <drizzled/item/float.h>
#include <drizzled/item/null.h>
#include <drizzled/temporal.h>

#include <drizzled/field.h>
#include <drizzled/field/str.h>
#include <drizzled/field/num.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/boolean.h>
#include <drizzled/field/enum.h>
#include <drizzled/field/null.h>
#include <drizzled/field/date.h>
#include <drizzled/field/decimal.h>
#include <drizzled/field/real.h>
#include <drizzled/field/double.h>
#include <drizzled/field/int32.h>
#include <drizzled/field/int64.h>
#include <drizzled/field/size.h>
#include <drizzled/field/num.h>
#include <drizzled/field/time.h>
#include <drizzled/field/epoch.h>
#include <drizzled/field/datetime.h>
#include <drizzled/field/microtime.h>
#include <drizzled/field/varstring.h>
#include <drizzled/field/uuid.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/definition/cache.h>
#include <drizzled/typelib.h>
#include <drizzled/key.h>
#include <drizzled/open_tables_state.h>

using namespace std;

namespace drizzled {

extern size_t table_def_size;

static enum_field_types proto_field_type_to_drizzle_type(const message::Table::Field &field)
{
  switch(field.type())
  {
  case message::Table::Field::INTEGER:
    return DRIZZLE_TYPE_LONG;

  case message::Table::Field::DOUBLE:
    return DRIZZLE_TYPE_DOUBLE;

  case message::Table::Field::EPOCH:
    if (field.has_time_options() and field.time_options().microseconds())
      return DRIZZLE_TYPE_MICROTIME;

    return DRIZZLE_TYPE_TIMESTAMP;

  case message::Table::Field::BIGINT:
    return DRIZZLE_TYPE_LONGLONG;

  case message::Table::Field::DATETIME:
    return DRIZZLE_TYPE_DATETIME;

  case message::Table::Field::DATE:
    return DRIZZLE_TYPE_DATE;

  case message::Table::Field::VARCHAR:
    return DRIZZLE_TYPE_VARCHAR;

  case message::Table::Field::DECIMAL:
    return DRIZZLE_TYPE_DECIMAL;

  case message::Table::Field::ENUM:
    return DRIZZLE_TYPE_ENUM;

  case message::Table::Field::BLOB:
    return DRIZZLE_TYPE_BLOB;

  case message::Table::Field::UUID:
    return  DRIZZLE_TYPE_UUID;

  case message::Table::Field::BOOLEAN:
    return DRIZZLE_TYPE_BOOLEAN;

  case message::Table::Field::TIME:
    return DRIZZLE_TYPE_TIME;
  }

  abort();
}

static Item *default_value_item(enum_field_types field_type,
                                const charset_info_st *charset,
                                bool default_null, const string *default_value,
                                const string *default_bin_value)
{
  Item *default_item= NULL;
  int error= 0;

  if (default_null)
  {
    return new Item_null();
  }

  switch(field_type)
  {
  case DRIZZLE_TYPE_LONG:
  case DRIZZLE_TYPE_LONGLONG:
    default_item= new Item_int(default_value->c_str(),
                               (int64_t) internal::my_strtoll10(default_value->c_str(),
                                                                NULL,
                                                                &error),
                               default_value->length());

    if (error && error != -1) /* was an error and wasn't a negative number */
    {
      delete default_item;
      return NULL;
    }

    break;
  case DRIZZLE_TYPE_DOUBLE:
    default_item= new Item_float(default_value->c_str(),
                                 default_value->length());
    break;
  case DRIZZLE_TYPE_NULL:
    assert(0);
    abort();
  case DRIZZLE_TYPE_TIMESTAMP:
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_TIME:
  case DRIZZLE_TYPE_DATE:
  case DRIZZLE_TYPE_ENUM:
  case DRIZZLE_TYPE_UUID:
  case DRIZZLE_TYPE_MICROTIME:
  case DRIZZLE_TYPE_BOOLEAN:
    default_item= new Item_string(default_value->c_str(),
                                  default_value->length(),
                                  system_charset_info);
    break;
  case DRIZZLE_TYPE_VARCHAR:
  case DRIZZLE_TYPE_BLOB: /* Blob is here due to TINYTEXT. Feel the hate. */
    if (charset==&my_charset_bin)
    {
      default_item= new Item_string(default_bin_value->c_str(),
                                    default_bin_value->length(),
                                    &my_charset_bin);
    }
    else
    {
      default_item= new Item_string(default_value->c_str(),
                                    default_value->length(),
                                    system_charset_info);
    }
    break;
  case DRIZZLE_TYPE_DECIMAL:
    default_item= new Item_decimal(default_value->c_str(),
                                   default_value->length(),
                                   system_charset_info);
    break;
  }

  return default_item;
}



/**
 * @todo
 *
 * Precache this stuff....
 */
bool TableShare::fieldInPrimaryKey(Field *in_field) const
{
  assert(getTableMessage());

  size_t num_indexes= getTableMessage()->indexes_size();

  for (size_t x= 0; x < num_indexes; ++x)
  {
    const message::Table::Index &index= getTableMessage()->indexes(x);
    if (index.is_primary())
    {
      size_t num_parts= index.index_part_size();
      for (size_t y= 0; y < num_parts; ++y)
      {
        if (index.index_part(y).fieldnr() == in_field->position())
          return true;
      }
    }
  }
  return false;
}

TableShare::TableShare(const identifier::Table::Type type_arg) :
  table_category(TABLE_UNKNOWN_CATEGORY),
  found_next_number_field(NULL),
  timestamp_field(NULL),
  key_info(NULL),
  mem_root(TABLE_ALLOC_BLOCK_SIZE),
  all_set(),
  db(NULL_LEX_STRING),
  table_name(NULL_LEX_STRING),
  path(NULL_LEX_STRING),
  normalized_path(NULL_LEX_STRING),
  block_size(0),
  version(0),
  timestamp_offset(0),
  reclength(0),
  stored_rec_length(0),
  max_rows(0),
  _table_message(NULL),
  storage_engine(NULL),
  tmp_table(type_arg),
  _ref_count(0),
  null_bytes(0),
  last_null_bit_pos(0),
  _field_size(0),
  rec_buff_length(0),
  keys(0),
  key_parts(0),
  max_key_length(0),
  max_unique_length(0),
  total_key_length(0),
  uniques(0),
  null_fields(0),
  blob_fields(0),
  has_variable_width(false),
  db_create_options(0),
  db_options_in_use(0),
  db_record_offset(0),
  rowid_field_offset(0),
  primary_key(MAX_KEY),
  next_number_index(0),
  next_number_key_offset(0),
  next_number_keypart(0),
  error(0),
  open_errno(0),
  errarg(0),
  blob_ptr_size(portable_sizeof_char_ptr),
  db_low_byte_first(false),
  keys_in_use(0),
  keys_for_keyread(0)
{
  if (type_arg == message::Table::INTERNAL)
  {
    identifier::Table::build_tmptable_filename(private_key_for_cache.vectorPtr());
    init(private_key_for_cache.vector(), private_key_for_cache.vector());
  }
  else
  {
    init("", "");
  }
}

TableShare::TableShare(const identifier::Table &identifier, const identifier::Table::Key &key) :// Used by placeholder
  table_category(TABLE_UNKNOWN_CATEGORY),
  found_next_number_field(NULL),
  timestamp_field(NULL),
  key_info(NULL),
  mem_root(TABLE_ALLOC_BLOCK_SIZE),
  table_charset(0),
  all_set(),
  db(NULL_LEX_STRING),
  table_name(NULL_LEX_STRING),
  path(NULL_LEX_STRING),
  normalized_path(NULL_LEX_STRING),
  block_size(0),
  version(0),
  timestamp_offset(0),
  reclength(0),
  stored_rec_length(0),
  max_rows(0),
  _table_message(NULL),
  storage_engine(NULL),
  tmp_table(message::Table::INTERNAL),
  _ref_count(0),
  null_bytes(0),
  last_null_bit_pos(0),
  _field_size(0),
  rec_buff_length(0),
  keys(0),
  key_parts(0),
  max_key_length(0),
  max_unique_length(0),
  total_key_length(0),
  uniques(0),
  null_fields(0),
  blob_fields(0),
  has_variable_width(false),
  db_create_options(0),
  db_options_in_use(0),
  db_record_offset(0),
  rowid_field_offset(0),
  primary_key(MAX_KEY),
  next_number_index(0),
  next_number_key_offset(0),
  next_number_keypart(0),
  error(0),
  open_errno(0),
  errarg(0),
  blob_ptr_size(portable_sizeof_char_ptr),
  db_low_byte_first(false),
  keys_in_use(0),
  keys_for_keyread(0)
{
  assert(identifier.getKey() == key);

  private_key_for_cache= key;

  table_category=         TABLE_CATEGORY_TEMPORARY;
  tmp_table=              message::Table::INTERNAL;

  db.str= const_cast<char *>(private_key_for_cache.vector());
  db.length= strlen(private_key_for_cache.vector());

  table_name.str= const_cast<char *>(private_key_for_cache.vector()) + strlen(private_key_for_cache.vector()) + 1;
  table_name.length= strlen(table_name.str);
  path.str= (char *)"";
  normalized_path.str= path.str;
  path.length= normalized_path.length= 0;

  std::string tb_name(identifier.getTableName());
  std::transform(tb_name.begin(), tb_name.end(), tb_name.begin(), ::tolower);
  assert(strcmp(tb_name.c_str(), table_name.str) == 0);

  assert(strcmp(identifier.getSchemaName().c_str(), db.str) == 0);
}


TableShare::TableShare(const identifier::Table &identifier) : // Just used during createTable()
  table_category(TABLE_UNKNOWN_CATEGORY),
  found_next_number_field(NULL),
  timestamp_field(NULL),
  key_info(NULL),
  mem_root(TABLE_ALLOC_BLOCK_SIZE),
  table_charset(0),
  all_set(),
  db(NULL_LEX_STRING),
  table_name(NULL_LEX_STRING),
  path(NULL_LEX_STRING),
  normalized_path(NULL_LEX_STRING),
  block_size(0),
  version(0),
  timestamp_offset(0),
  reclength(0),
  stored_rec_length(0),
  max_rows(0),
  _table_message(NULL),
  storage_engine(NULL),
  tmp_table(identifier.getType()),
  _ref_count(0),
  null_bytes(0),
  last_null_bit_pos(0),
  _field_size(0),
  rec_buff_length(0),
  keys(0),
  key_parts(0),
  max_key_length(0),
  max_unique_length(0),
  total_key_length(0),
  uniques(0),
  null_fields(0),
  blob_fields(0),
  has_variable_width(false),
  db_create_options(0),
  db_options_in_use(0),
  db_record_offset(0),
  rowid_field_offset(0),
  primary_key(MAX_KEY),
  next_number_index(0),
  next_number_key_offset(0),
  next_number_keypart(0),
  error(0),
  open_errno(0),
  errarg(0),
  blob_ptr_size(portable_sizeof_char_ptr),
  db_low_byte_first(false),
  keys_in_use(0),
  keys_for_keyread(0)
{
  private_key_for_cache= identifier.getKey();
  assert(identifier.getPath().size()); // Since we are doing a create table, this should be a positive value
  private_normalized_path.resize(identifier.getPath().size() + 1);
  memcpy(&private_normalized_path[0], identifier.getPath().c_str(), identifier.getPath().size());

  {
    table_category=         TABLE_CATEGORY_TEMPORARY;
    tmp_table=              message::Table::INTERNAL;
    db.str= const_cast<char *>(private_key_for_cache.vector());
    db.length= strlen(private_key_for_cache.vector());
    table_name.str= db.str + 1;
    table_name.length= strlen(table_name.str);
    path.str= &private_normalized_path[0];
    normalized_path.str= path.str;
    path.length= normalized_path.length= private_normalized_path.size();
  }
}


/*
  Used for shares that will go into the cache.
*/
TableShare::TableShare(const identifier::Table::Type type_arg,
                       const identifier::Table &identifier,
                       char *path_arg,
                       uint32_t path_length_arg) :
  table_category(TABLE_UNKNOWN_CATEGORY),
  found_next_number_field(NULL),
  timestamp_field(NULL),
  key_info(NULL),
  mem_root(TABLE_ALLOC_BLOCK_SIZE),
  table_charset(0),
  all_set(),
  db(NULL_LEX_STRING),
  table_name(NULL_LEX_STRING),
  path(NULL_LEX_STRING),
  normalized_path(NULL_LEX_STRING),
  block_size(0),
  version(0),
  timestamp_offset(0),
  reclength(0),
  stored_rec_length(0),
  max_rows(0),
  _table_message(NULL),
  storage_engine(NULL),
  tmp_table(type_arg),
  _ref_count(0),
  null_bytes(0),
  last_null_bit_pos(0),
  _field_size(0),
  rec_buff_length(0),
  keys(0),
  key_parts(0),
  max_key_length(0),
  max_unique_length(0),
  total_key_length(0),
  uniques(0),
  null_fields(0),
  blob_fields(0),
  has_variable_width(false),
  db_create_options(0),
  db_options_in_use(0),
  db_record_offset(0),
  rowid_field_offset(0),
  primary_key(MAX_KEY),
  next_number_index(0),
  next_number_key_offset(0),
  next_number_keypart(0),
  error(0),
  open_errno(0),
  errarg(0),
  blob_ptr_size(portable_sizeof_char_ptr),
  db_low_byte_first(false),
  keys_in_use(0),
  keys_for_keyread(0)
{
  char *path_buff;
  std::string _path;

  private_key_for_cache= identifier.getKey();
  /*
    Let us use the fact that the key is "db/0/table_name/0" + optional
    part for temporary tables.
  */
  db.str= const_cast<char *>(private_key_for_cache.vector());
  db.length=         strlen(db.str);
  table_name.str=    db.str + db.length + 1;
  table_name.length= strlen(table_name.str);

  if (path_arg)
  {
    _path.append(path_arg, path_length_arg);
  }
  else
  {
    identifier::Table::build_table_filename(_path, db.str, table_name.str, false);
  }

  path_buff= (char *)mem_root.alloc(_path.length() + 1);
  setPath(path_buff, _path.length());
  strcpy(path_buff, _path.c_str());
  setNormalizedPath(path_buff, _path.length());

  version= g_refresh_version;
}

void TableShare::init(const char *new_table_name,
                      const char *new_path)
{

  table_category=         TABLE_CATEGORY_TEMPORARY;
  tmp_table=              message::Table::INTERNAL;
  db.str= (char *)"";
  db.length= 0;
  table_name.str=         (char*) new_table_name;
  table_name.length=      strlen(new_table_name);
  path.str=               (char*) new_path;
  normalized_path.str=    (char*) new_path;
  path.length= normalized_path.length= strlen(new_path);
}

TableShare::~TableShare() 
{
  storage_engine= NULL;

  mem_root.free_root(MYF(0));                 // Free's share
}

void TableShare::setIdentifier(const identifier::Table &identifier_arg)
{
  private_key_for_cache= identifier_arg.getKey();

  /*
    Let us use the fact that the key is "db/0/table_name/0" + optional
    part for temporary tables.
  */
  db.str= const_cast<char *>(private_key_for_cache.vector());
  db.length=         strlen(db.str);
  table_name.str=    db.str + db.length + 1;
  table_name.length= strlen(table_name.str);

  getTableMessage()->set_name(identifier_arg.getTableName());
  getTableMessage()->set_schema(identifier_arg.getSchemaName());
}

bool TableShare::parse_table_proto(Session& session, const message::Table &table)
{
  drizzled::error_t local_error= EE_OK;

  if (! table.IsInitialized())
  {
    my_error(ER_CORRUPT_TABLE_DEFINITION, MYF(0),
             table.name().empty() ? " " :  table.name().c_str(),
             table.InitializationErrorString().c_str());

    return ER_CORRUPT_TABLE_DEFINITION;
  }

  setTableMessage(table);

  storage_engine= plugin::StorageEngine::findByName(session, table.engine().name());
  assert(storage_engine); // We use an assert() here because we should never get this far and still have no suitable engine.

  message::Table::TableOptions table_options;

  if (table.has_options())
    table_options= table.options();

  uint32_t local_db_create_options= 0;

  if (table_options.pack_record())
    local_db_create_options|= HA_OPTION_PACK_RECORD;

  /* local_db_create_options was stored as 2 bytes in FRM
    Any HA_OPTION_ that doesn't fit into 2 bytes was silently truncated away.
  */
  db_create_options= (local_db_create_options & 0x0000FFFF);
  db_options_in_use= db_create_options;

  block_size= table_options.has_block_size() ?
    table_options.block_size() : 0;

  table_charset= get_charset(table_options.collation_id());

  if (not table_charset)
  {
    my_error(ER_CORRUPT_TABLE_DEFINITION_UNKNOWN_COLLATION, MYF(0),
             table_options.collation().c_str(),
             table.name().c_str());

    return ER_CORRUPT_TABLE_DEFINITION; // Historical
  }

  db_record_offset= 1;

  keys= table.indexes_size();

  key_parts= 0;
  for (int indx= 0; indx < table.indexes_size(); indx++)
    key_parts+= table.indexes(indx).index_part_size();

  key_info= (KeyInfo*) alloc(table.indexes_size() * sizeof(KeyInfo) +key_parts*sizeof(KeyPartInfo));

  KeyPartInfo *key_part;

  key_part= reinterpret_cast<KeyPartInfo*>
    (key_info+table.indexes_size());


  ulong *rec_per_key= (ulong*) alloc(sizeof(ulong*)*key_parts);

  KeyInfo* keyinfo= key_info;
  for (int keynr= 0; keynr < table.indexes_size(); keynr++, keyinfo++)
  {
    message::Table::Index indx= table.indexes(keynr);

    keyinfo->table= 0;
    keyinfo->flags= 0;

    if (indx.is_unique())
      keyinfo->flags|= HA_NOSAME;

    if (indx.has_options())
    {
      message::Table::Index::Options indx_options= indx.options();
      if (indx_options.pack_key())
        keyinfo->flags|= HA_PACK_KEY;

      if (indx_options.var_length_key())
        keyinfo->flags|= HA_VAR_LENGTH_PART;

      if (indx_options.null_part_key())
        keyinfo->flags|= HA_NULL_PART_KEY;

      if (indx_options.binary_pack_key())
        keyinfo->flags|= HA_BINARY_PACK_KEY;

      if (indx_options.has_partial_segments())
        keyinfo->flags|= HA_KEY_HAS_PART_KEY_SEG;

      if (indx_options.auto_generated_key())
        keyinfo->flags|= HA_GENERATED_KEY;

      if (indx_options.has_key_block_size())
      {
        keyinfo->flags|= HA_USES_BLOCK_SIZE;
        keyinfo->block_size= indx_options.key_block_size();
      }
      else
      {
        keyinfo->block_size= 0;
      }
    }

    switch (indx.type())
    {
    case message::Table::Index::UNKNOWN_INDEX:
      keyinfo->algorithm= HA_KEY_ALG_UNDEF;
      break;
    case message::Table::Index::BTREE:
      keyinfo->algorithm= HA_KEY_ALG_BTREE;
      break;
    case message::Table::Index::HASH:
      keyinfo->algorithm= HA_KEY_ALG_HASH;
      break;

    default:
      /* TODO: suitable warning ? */
      keyinfo->algorithm= HA_KEY_ALG_UNDEF;
      break;
    }

    keyinfo->key_length= indx.key_length();

    keyinfo->key_parts= indx.index_part_size();

    keyinfo->key_part= key_part;
    keyinfo->rec_per_key= rec_per_key;

    for (unsigned int partnr= 0;
         partnr < keyinfo->key_parts;
         partnr++, key_part++)
    {
      message::Table::Index::IndexPart part;
      part= indx.index_part(partnr);

      *rec_per_key++= 0;

      key_part->field= NULL;
      key_part->fieldnr= part.fieldnr() + 1; // start from 1.
      key_part->null_bit= 0;
      /* key_part->null_offset is only set if null_bit (see later) */
      /* key_part->key_type= */ /* I *THINK* this may be okay.... */
      /* key_part->type ???? */
      key_part->key_part_flag= 0;
      if (part.has_in_reverse_order())
        key_part->key_part_flag= part.in_reverse_order()? HA_REVERSE_SORT : 0;

      key_part->length= part.compare_length();

      int mbmaxlen= 1;

      if (table.field(part.fieldnr()).type() == message::Table::Field::VARCHAR
          || table.field(part.fieldnr()).type() == message::Table::Field::BLOB)
      {
        uint32_t collation_id;

        if (table.field(part.fieldnr()).string_options().has_collation_id())
          collation_id= table.field(part.fieldnr()).string_options().collation_id();
        else
          collation_id= table.options().collation_id();

        const charset_info_st *cs= get_charset(collation_id);

        mbmaxlen= cs->mbmaxlen;
      }
      key_part->length*= mbmaxlen;

      key_part->store_length= key_part->length;

      /* key_part->offset is set later */
      key_part->key_type= 0;
    }

    if (! indx.has_comment())
    {
      keyinfo->comment.length= 0;
      keyinfo->comment.str= NULL;
    }
    else
    {
      keyinfo->flags|= HA_USES_COMMENT;
      keyinfo->comment.length= indx.comment().length();
      keyinfo->comment.str= strmake(indx.comment().c_str(), keyinfo->comment.length);
    }

    keyinfo->name= strmake(indx.name().c_str(), indx.name().length());

    addKeyName(string(keyinfo->name, indx.name().length()));
  }

  keys_for_keyread.reset();
  set_prefix(keys_in_use, keys);

  _field_size= table.field_size();

  setFields(_field_size + 1);
  _fields[_field_size]= NULL;

  uint32_t local_null_fields= 0;
  reclength= 0;

  std::vector<uint32_t> field_offsets;
  std::vector<uint32_t> field_pack_length;

  field_offsets.resize(_field_size);
  field_pack_length.resize(_field_size);

  uint32_t interval_count= 0;
  uint32_t interval_parts= 0;

  uint32_t stored_columns_reclength= 0;

  for (unsigned int fieldnr= 0; fieldnr < _field_size; fieldnr++)
  {
    message::Table::Field pfield= table.field(fieldnr);
    if (pfield.constraints().is_nullable()) // Historical reference
    {
      local_null_fields++;
    }
    else if (not pfield.constraints().is_notnull())
    {
      local_null_fields++;
    }

    enum_field_types drizzle_field_type= proto_field_type_to_drizzle_type(pfield);

    field_offsets[fieldnr]= stored_columns_reclength;

    /* the below switch is very similar to
      CreateField::create_length_to_internal_length in field.cc
      (which should one day be replace by just this code)
    */
    switch(drizzle_field_type)
    {
    case DRIZZLE_TYPE_BLOB:
    case DRIZZLE_TYPE_VARCHAR:
      {
        message::Table::Field::StringFieldOptions field_options= pfield.string_options();

        const charset_info_st *cs= get_charset(field_options.has_collation_id() ?
                                            field_options.collation_id() : 0);

        if (! cs)
          cs= default_charset_info;

        field_pack_length[fieldnr]= calc_pack_length(drizzle_field_type,
                                                     field_options.length() * cs->mbmaxlen);
      }
      break;
    case DRIZZLE_TYPE_ENUM:
      {
        message::Table::Field::EnumerationValues field_options= pfield.enumeration_values();

        field_pack_length[fieldnr]= 4;

        interval_count++;
        interval_parts+= field_options.field_value_size();
      }
      break;
    case DRIZZLE_TYPE_DECIMAL:
      {
        message::Table::Field::NumericFieldOptions fo= pfield.numeric_options();

        field_pack_length[fieldnr]= class_decimal_get_binary_size(fo.precision(), fo.scale());
      }
      break;
    default:
      /* Zero is okay here as length is fixed for other types. */
      field_pack_length[fieldnr]= calc_pack_length(drizzle_field_type, 0);
    }

    reclength+= field_pack_length[fieldnr];
    stored_columns_reclength+= field_pack_length[fieldnr];
  }

  /* data_offset added to stored_rec_length later */
  stored_rec_length= stored_columns_reclength;

  null_fields= local_null_fields;

  ulong null_bits= local_null_fields;
  if (! table_options.pack_record())
    null_bits++;
  ulong data_offset= (null_bits + 7)/8;


  reclength+= data_offset;
  stored_rec_length+= data_offset;

  ulong local_rec_buff_length;

  local_rec_buff_length= ALIGN_SIZE(reclength + 1);
  rec_buff_length= local_rec_buff_length;

  resizeDefaultValues(local_rec_buff_length);
  unsigned char* record= getDefaultValues();
  int null_count= 0;

  if (! table_options.pack_record())
  {
    null_count++; // one bit for delete mark.
    *record|= 1;
  }


  intervals.resize(interval_count);

  /* Now fix the TYPELIBs for the intervals (enum values)
    and field names.
  */

  uint32_t interval_nr= 0;

  for (unsigned int fieldnr= 0; fieldnr < _field_size; fieldnr++)
  {
    message::Table::Field pfield= table.field(fieldnr);

    /* enum typelibs */
    if (pfield.type() != message::Table::Field::ENUM)
      continue;

    message::Table::Field::EnumerationValues field_options= pfield.enumeration_values();

    if (field_options.field_value_size() > Field_enum::max_supported_elements)
    {
      my_error(ER_CORRUPT_TABLE_DEFINITION_ENUM, MYF(0), table.name().c_str());

      return ER_CORRUPT_TABLE_DEFINITION_ENUM; // Historical
    }


    const charset_info_st *charset= get_charset(field_options.has_collation_id() ?
                                             field_options.collation_id() : 0);

    if (! charset)
      charset= default_charset_info;

    TYPELIB *t= (&intervals[interval_nr]);

    t->type_names= (const char**)alloc((field_options.field_value_size() + 1) * sizeof(char*));

    t->type_lengths= (unsigned int*)alloc((field_options.field_value_size() + 1) * sizeof(unsigned int));

    t->type_names[field_options.field_value_size()]= NULL;
    t->type_lengths[field_options.field_value_size()]= 0;

    t->count= field_options.field_value_size();
    t->name= NULL;

    for (int n= 0; n < field_options.field_value_size(); n++)
    {
      t->type_names[n]= strmake(field_options.field_value(n).c_str(), field_options.field_value(n).length());

      /* 
       * Go ask the charset what the length is as for "" length=1
       * and there's stripping spaces or some other crack going on.
     */
      uint32_t lengthsp;
      lengthsp= charset->cset->lengthsp(charset,
                                        t->type_names[n],
                                        field_options.field_value(n).length());
      t->type_lengths[n]= lengthsp;
    }
    interval_nr++;
  }


  /* and read the fields */
  interval_nr= 0;

  bool use_hash= _field_size >= MAX_FIELDS_BEFORE_HASH;

  unsigned char* null_pos= getDefaultValues();
  int null_bit_pos= (table_options.pack_record()) ? 0 : 1;

  for (unsigned int fieldnr= 0; fieldnr < _field_size; fieldnr++)
  {
    message::Table::Field pfield= table.field(fieldnr);

    Field::utype unireg_type= Field::NONE;

    if (pfield.has_numeric_options() &&
        pfield.numeric_options().is_autoincrement())
    {
      unireg_type= Field::NEXT_NUMBER;
    }

    if (pfield.has_options() &&
        pfield.options().has_default_expression() &&
        pfield.options().default_expression().compare("CURRENT_TIMESTAMP") == 0)
    {
      if (pfield.options().has_update_expression() &&
          pfield.options().update_expression().compare("CURRENT_TIMESTAMP") == 0)
      {
        unireg_type= Field::TIMESTAMP_DNUN_FIELD;
      }
      else if (! pfield.options().has_update_expression())
      {
        unireg_type= Field::TIMESTAMP_DN_FIELD;
      }
      else
      {
        assert(0); // Invalid update value.
        abort();
      }
    }
    else if (pfield.has_options() &&
             pfield.options().has_update_expression() &&
             pfield.options().update_expression().compare("CURRENT_TIMESTAMP") == 0)
    {
      unireg_type= Field::TIMESTAMP_UN_FIELD;
    }

    LEX_STRING comment;
    if (!pfield.has_comment())
    {
      comment.str= (char*)"";
      comment.length= 0;
    }
    else
    {
      size_t len= pfield.comment().length();
      const char* str= pfield.comment().c_str();

      comment.str= strmake(str, len);
      comment.length= len;
    }

    enum_field_types field_type;

    field_type= proto_field_type_to_drizzle_type(pfield);

    const charset_info_st *charset= &my_charset_bin;

    if (field_type == DRIZZLE_TYPE_BLOB ||
        field_type == DRIZZLE_TYPE_VARCHAR)
    {
      message::Table::Field::StringFieldOptions field_options= pfield.string_options();

      charset= get_charset(field_options.has_collation_id() ?
                           field_options.collation_id() : 0);

      if (! charset)
        charset= default_charset_info;
    }

    if (field_type == DRIZZLE_TYPE_ENUM)
    {
      message::Table::Field::EnumerationValues field_options= pfield.enumeration_values();

      charset= get_charset(field_options.has_collation_id()?
                           field_options.collation_id() : 0);

      if (! charset)
        charset= default_charset_info;
    }

    uint8_t decimals= 0;
    if (field_type == DRIZZLE_TYPE_DECIMAL
        || field_type == DRIZZLE_TYPE_DOUBLE)
    {
      message::Table::Field::NumericFieldOptions fo= pfield.numeric_options();

      if (! pfield.has_numeric_options() || ! fo.has_scale())
      {
        /*
          We don't write the default to table proto so
          if no decimals specified for DOUBLE, we use the default.
        */
        decimals= NOT_FIXED_DEC;
      }
      else
      {
        if (fo.scale() > DECIMAL_MAX_SCALE)
        {
          local_error= ER_NOT_FORM_FILE;

          return true;
        }
        decimals= static_cast<uint8_t>(fo.scale());
      }
    }

    Item *default_value= NULL;

    if (pfield.options().has_default_value() ||
        pfield.options().default_null()  ||
        pfield.options().has_default_bin_value())
    {
      default_value= default_value_item(field_type,
                                        charset,
                                        pfield.options().default_null(),
                                        &pfield.options().default_value(),
                                        &pfield.options().default_bin_value());
      if (default_value == NULL)
      {
        my_error(ER_INVALID_DEFAULT, MYF(0), pfield.name().c_str());
        return true;
      }
    }


    uint32_t field_length= 0; //Assignment is for compiler complaint.

    // We set field_length in this loop.
    switch (field_type)
    {
    case DRIZZLE_TYPE_BLOB:
    case DRIZZLE_TYPE_VARCHAR:
      {
        message::Table::Field::StringFieldOptions field_options= pfield.string_options();

        charset= get_charset(field_options.has_collation_id() ?
                             field_options.collation_id() : 0);

        if (! charset)
          charset= default_charset_info;

        field_length= field_options.length() * charset->mbmaxlen;
      }
      break;
    case DRIZZLE_TYPE_DOUBLE:
      {
        message::Table::Field::NumericFieldOptions fo= pfield.numeric_options();
        if (!fo.has_precision() && !fo.has_scale())
        {
          field_length= DBL_DIG+7;
        }
        else
        {
          field_length= fo.precision();
        }
        if (field_length < decimals &&
            decimals != NOT_FIXED_DEC)
        {
          my_error(ER_M_BIGGER_THAN_D, MYF(0), pfield.name().c_str());
          local_error= ER_M_BIGGER_THAN_D;
          return true;
        }
        break;
      }
    case DRIZZLE_TYPE_DECIMAL:
      {
        message::Table::Field::NumericFieldOptions fo= pfield.numeric_options();

        field_length= class_decimal_precision_to_length(fo.precision(), fo.scale(),
                                                     false);
        break;
      }
    case DRIZZLE_TYPE_DATETIME:
      field_length= DateTime::MAX_STRING_LENGTH;
      break;
    case DRIZZLE_TYPE_DATE:
      field_length= Date::MAX_STRING_LENGTH;
      break;
    case DRIZZLE_TYPE_ENUM:
      {
        field_length= 0;

        message::Table::Field::EnumerationValues fo= pfield.enumeration_values();

        for (int valnr= 0; valnr < fo.field_value_size(); valnr++)
        {
          if (fo.field_value(valnr).length() > field_length)
          {
            field_length= charset->cset->numchars(charset,
                                                  fo.field_value(valnr).c_str(),
                                                  fo.field_value(valnr).c_str()
                                                  + fo.field_value(valnr).length())
              * charset->mbmaxlen;
          }
        }
      }
      break;
    case DRIZZLE_TYPE_LONG:
      {
        uint32_t sign_len= pfield.constraints().is_unsigned() ? 0 : 1;
        field_length= MAX_INT_WIDTH+sign_len;
      }
      break;
    case DRIZZLE_TYPE_LONGLONG:
      {
        uint32_t sign_len= pfield.constraints().is_unsigned() ? 0 : 1;
        field_length= MAX_BIGINT_WIDTH+sign_len;
      }
      break;
    case DRIZZLE_TYPE_UUID:
      field_length= field::Uuid::max_string_length();
      break;
    case DRIZZLE_TYPE_BOOLEAN:
      field_length= field::Boolean::max_string_length();
      break;
    case DRIZZLE_TYPE_MICROTIME:
      field_length= field::Microtime::max_string_length();
      break;
    case DRIZZLE_TYPE_TIMESTAMP:
      field_length= field::Epoch::max_string_length();
      break;
    case DRIZZLE_TYPE_TIME:
      field_length= field::Time::max_string_length();
      break;
    case DRIZZLE_TYPE_NULL:
      abort(); // Programming error
    }

    bool is_not_null= false;

    if (not pfield.constraints().is_nullable())
    {
      is_not_null= true;
    }
    else if (pfield.constraints().is_notnull())
    {
      is_not_null= true;
    }

    Field* f= make_field(pfield,
                         record + field_offsets[fieldnr] + data_offset,
                         field_length,
                         not is_not_null,
                         null_pos,
                         null_bit_pos,
                         decimals,
                         field_type,
                         charset,
                         MTYP_TYPENR(unireg_type),
                         ((field_type == DRIZZLE_TYPE_ENUM) ?  &intervals[interval_nr++] : (TYPELIB*) 0),
                         getTableMessage()->field(fieldnr).name().c_str());

    _fields[fieldnr]= f;

    // Insert post make_field code here.
    switch (field_type)
    {
    case DRIZZLE_TYPE_BLOB:
    case DRIZZLE_TYPE_VARCHAR:
    case DRIZZLE_TYPE_DOUBLE:
    case DRIZZLE_TYPE_DECIMAL:
    case DRIZZLE_TYPE_TIMESTAMP:
    case DRIZZLE_TYPE_TIME:
    case DRIZZLE_TYPE_DATETIME:
    case DRIZZLE_TYPE_MICROTIME:
    case DRIZZLE_TYPE_DATE:
    case DRIZZLE_TYPE_ENUM:
    case DRIZZLE_TYPE_LONG:
    case DRIZZLE_TYPE_LONGLONG:
    case DRIZZLE_TYPE_NULL:
    case DRIZZLE_TYPE_UUID:
    case DRIZZLE_TYPE_BOOLEAN:
      break;
    }

    // This needs to go, we should be setting the "use" on the field so that
    // it does not reference the share/table.
    table::Shell temp_table(*this); /* Use this so that BLOB DEFAULT '' works */
    temp_table.in_use= &session;

    f->init(&temp_table); /* blob default values need table obj */

    if (! (f->flags & NOT_NULL_FLAG))
    {
      *f->null_ptr|= f->null_bit;
      if (! (null_bit_pos= (null_bit_pos + 1) & 7)) /* @TODO Ugh. */
        null_pos++;
      null_count++;
    }

    if (default_value)
    {
      enum_check_fields old_count_cuted_fields= session.count_cuted_fields;
      session.count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;
      int res= default_value->save_in_field(f, 1);
      session.count_cuted_fields= old_count_cuted_fields;
      if (res != 0 && res != 3) /* @TODO Huh? */
      {
        my_error(ER_INVALID_DEFAULT, MYF(0), f->field_name);
        local_error= ER_INVALID_DEFAULT;

        return true;
      }
    }
    else if (f->real_type() == DRIZZLE_TYPE_ENUM && (f->flags & NOT_NULL_FLAG))
    {
      f->set_notnull();
      f->store((int64_t) 1, true);
    }
    else
    {
      f->reset();
    }

    /* hack to undo f->init() */
    f->setTable(NULL);
    f->orig_table= NULL;

    f->setPosition(fieldnr);
    f->comment= comment;
    if (not default_value &&
        not (f->unireg_check==Field::NEXT_NUMBER) &&
        (f->flags & NOT_NULL_FLAG) &&
        (not f->is_timestamp()))
    {
      f->flags|= NO_DEFAULT_VALUE_FLAG;
    }

    if (f->unireg_check == Field::NEXT_NUMBER)
      found_next_number_field= &(_fields[fieldnr]);

    if (use_hash) /* supposedly this never fails... but comments lie */
    {
      const char *local_field_name= _fields[fieldnr]->field_name;
      name_hash.insert(make_pair(local_field_name, &(_fields[fieldnr])));
    }
  }

  keyinfo= key_info;
  for (unsigned int keynr= 0; keynr < keys; keynr++, keyinfo++)
  {
    key_part= keyinfo->key_part;

    for (unsigned int partnr= 0;
         partnr < keyinfo->key_parts;
         partnr++, key_part++)
    {
      /* 
       * Fix up key_part->offset by adding data_offset.
       * We really should compute offset as well.
       * But at least this way we are a little better.
     */
      key_part->offset= field_offsets[key_part->fieldnr-1] + data_offset;
    }
  }

  /*
    We need to set the unused bits to 1. If the number of bits is a multiple
    of 8 there are no unused bits.
  */
  if (null_count & 7)
    *(record + null_count / 8)|= ~(((unsigned char) 1 << (null_count & 7)) - 1);

  null_bytes= (null_pos - (unsigned char*) record + (null_bit_pos + 7) / 8);

  last_null_bit_pos= null_bit_pos;

  /* Fix key stuff */
  if (key_parts)
  {
    uint32_t local_primary_key= 0;
    doesKeyNameExist("PRIMARY", local_primary_key);

    keyinfo= key_info;
    key_part= keyinfo->key_part;

    for (uint32_t key= 0; key < keys; key++,keyinfo++)
    {
      uint32_t usable_parts= 0;

      if (local_primary_key >= MAX_KEY && (keyinfo->flags & HA_NOSAME))
      {
        /*
          If the UNIQUE key doesn't have NULL columns and is not a part key
          declare this as a primary key.
        */
        local_primary_key=key;
        for (uint32_t i= 0; i < keyinfo->key_parts; i++)
        {
          uint32_t fieldnr= key_part[i].fieldnr;
          if (not fieldnr ||
              _fields[fieldnr-1]->null_ptr ||
              _fields[fieldnr-1]->key_length() != key_part[i].length)
          {
            local_primary_key= MAX_KEY; // Can't be used
            break;
          }
        }
      }

      for (uint32_t i= 0 ; i < keyinfo->key_parts ; key_part++,i++)
      {
        Field *local_field;
        if (! key_part->fieldnr)
        {
          return ENOMEM;
        }
        local_field= key_part->field= _fields[key_part->fieldnr-1];
        key_part->type= local_field->key_type();
        if (local_field->null_ptr)
        {
          key_part->null_offset=(uint32_t) ((unsigned char*) local_field->null_ptr - getDefaultValues());
          key_part->null_bit= local_field->null_bit;
          key_part->store_length+=HA_KEY_NULL_LENGTH;
          keyinfo->flags|=HA_NULL_PART_KEY;
          keyinfo->extra_length+= HA_KEY_NULL_LENGTH;
          keyinfo->key_length+= HA_KEY_NULL_LENGTH;
        }
        if (local_field->type() == DRIZZLE_TYPE_BLOB ||
            local_field->real_type() == DRIZZLE_TYPE_VARCHAR)
        {
          if (local_field->type() == DRIZZLE_TYPE_BLOB)
            key_part->key_part_flag|= HA_BLOB_PART;
          else
            key_part->key_part_flag|= HA_VAR_LENGTH_PART;
          keyinfo->extra_length+=HA_KEY_BLOB_LENGTH;
          key_part->store_length+=HA_KEY_BLOB_LENGTH;
          keyinfo->key_length+= HA_KEY_BLOB_LENGTH;
        }
        if (i == 0 && key != local_primary_key)
          local_field->flags |= (((keyinfo->flags & HA_NOSAME) &&
                                  (keyinfo->key_parts == 1)) ?
                                 UNIQUE_KEY_FLAG : MULTIPLE_KEY_FLAG);
        if (i == 0)
          local_field->key_start.set(key);
        if (local_field->key_length() == key_part->length &&
            !(local_field->flags & BLOB_FLAG))
        {
          enum ha_key_alg algo= key_info[key].algorithm;
          if (db_type()->index_flags(algo) & HA_KEYREAD_ONLY)
          {
            keys_for_keyread.set(key);
            local_field->part_of_key.set(key);
            local_field->part_of_key_not_clustered.set(key);
          }
          if (db_type()->index_flags(algo) & HA_READ_ORDER)
            local_field->part_of_sortkey.set(key);
        }
        if (!(key_part->key_part_flag & HA_REVERSE_SORT) &&
            usable_parts == i)
          usable_parts++;			// For FILESORT
        local_field->flags|= PART_KEY_FLAG;
        if (key == local_primary_key)
        {
          local_field->flags|= PRI_KEY_FLAG;
          /*
            If this field is part of the primary key and all keys contains
            the primary key, then we can use any key to find this column
          */
          if (storage_engine->check_flag(HTON_BIT_PRIMARY_KEY_IN_READ_INDEX))
          {
            local_field->part_of_key= keys_in_use;
            if (local_field->part_of_sortkey.test(key))
              local_field->part_of_sortkey= keys_in_use;
          }
        }
        if (local_field->key_length() != key_part->length)
        {
          key_part->key_part_flag|= HA_PART_KEY_SEG;
        }
      }
      keyinfo->usable_key_parts= usable_parts; // Filesort

      set_if_bigger(max_key_length,keyinfo->key_length+
                    keyinfo->key_parts);
      total_key_length+= keyinfo->key_length;

      if (keyinfo->flags & HA_NOSAME)
      {
        set_if_bigger(max_unique_length,keyinfo->key_length);
      }
    }
    if (local_primary_key < MAX_KEY &&
        (keys_in_use.test(local_primary_key)))
    {
      primary_key= local_primary_key;
      /*
        If we are using an integer as the primary key then allow the user to
        refer to it as '_rowid'
      */
      if (key_info[local_primary_key].key_parts == 1)
      {
        Field *local_field= key_info[local_primary_key].key_part[0].field;
        if (local_field && local_field->result_type() == INT_RESULT)
        {
          /* note that fieldnr here (and rowid_field_offset) starts from 1 */
          rowid_field_offset= (key_info[local_primary_key].key_part[0].
                                      fieldnr);
        }
      }
    }
  }

  if (found_next_number_field)
  {
    Field *reg_field= *found_next_number_field;
    if ((int) (next_number_index= (uint32_t)
               find_ref_key(key_info, keys,
                            getDefaultValues(), reg_field,
                            &next_number_key_offset,
                            &next_number_keypart)) < 0)
    {
      /* Wrong field definition */
      local_error= ER_NOT_FORM_FILE;

      return true;
    }
    else
    {
      reg_field->flags |= AUTO_INCREMENT_FLAG;
    }
  }

  if (blob_fields)
  {
    /* Store offsets to blob fields to find them fast */
    blob_field.resize(blob_fields);
    uint32_t *save= &blob_field[0];
    uint32_t k= 0;
    for (Fields::iterator iter= _fields.begin(); iter != _fields.end()-1; iter++, k++)
    {
      if ((*iter)->flags & BLOB_FLAG)
        (*save++)= k;
    }
  }

  all_set.clear();
  all_set.resize(_field_size);
  all_set.set();

  return local_error != EE_OK;
}

/*
  Read table definition from a binary / text based .frm cursor

  SYNOPSIS
  open_table_def()
  session		Thread Cursor
  share		Fill this with table definition

  NOTES
  This function is called when the table definition is not cached in
  definition::Cache::getCache()
  The data is returned in 'share', which is alloced by
  alloc_table_share().. The code assumes that share is initialized.

  RETURN VALUES
  0	ok
  1	Error (see open_table_error)
  2    Error (see open_table_error)
  3    Wrong data in .frm cursor
  4    Error (see open_table_error)
  5    Error (see open_table_error: charset unavailable)
  6    Unknown .frm version
*/

int TableShare::open_table_def(Session& session, const identifier::Table &identifier)
{
  drizzled::error_t local_error= EE_OK;

  message::table::shared_ptr table= plugin::StorageEngine::getTableMessage(session, identifier, local_error);

  if (table and table->IsInitialized())
  {
    if (parse_table_proto(session, *table))
    {
      local_error= ER_CORRUPT_TABLE_DEFINITION_UNKNOWN;
      my_error(ER_CORRUPT_TABLE_DEFINITION_UNKNOWN, identifier);
    }
    else
    {
      setTableCategory(TABLE_CATEGORY_USER);
      local_error= EE_OK;
    }
  }
  else if (table and not table->IsInitialized())
  {
    local_error= ER_CORRUPT_TABLE_DEFINITION_UNKNOWN;
    my_error(ER_CORRUPT_TABLE_DEFINITION_UNKNOWN, identifier);
  }
  else
  {
    local_error= ER_TABLE_UNKNOWN;
    my_error(ER_TABLE_UNKNOWN, identifier);
  }

  return static_cast<int>(local_error);
}


/*
  Open a table based on a TableShare

  SYNOPSIS
  open_table_from_share()
  session			Thread Cursor
  share		Table definition
  alias       	Alias for table
  db_stat		open flags (for example HA_OPEN_KEYFILE|
  HA_OPEN_RNDFILE..) can be 0 (example in
  ha_example_table)
  ha_open_flags	HA_OPEN_ABORT_IF_LOCKED etc..
  outparam       	result table

  RETURN VALUES
  0	ok
  1	Error (see open_table_error)
  2    Error (see open_table_error)
  3    Wrong data in .frm cursor
  4    Error (see open_table_error)
  5    Error (see open_table_error: charset unavailable)
  7    Table definition has changed in engine
*/
int TableShare::open_table_from_share(Session *session,
                                      const identifier::Table &identifier,
                                      const char *alias,
                                      uint32_t db_stat, uint32_t ha_open_flags,
                                      Table &outparam)
{
  bool error_reported= false;
  int ret= open_table_from_share_inner(session, alias, db_stat, outparam);

  if (not ret)
    ret= open_table_cursor_inner(identifier, db_stat, ha_open_flags, outparam, error_reported);

  if (not ret)
    return ret;

  if (not error_reported)
    open_table_error(ret, errno, 0);

  boost::checked_delete(outparam.cursor);
  outparam.cursor= 0;				// For easier error checking
  outparam.db_stat= 0;
  outparam.mem().free_root(MYF(0));       // Safe to call on zeroed root
  outparam.clearAlias();

  return ret;
}

int TableShare::open_table_from_share_inner(Session *session,
                                            const char *alias,
                                            uint32_t db_stat,
                                            Table &outparam)
{
  int local_error;
  uint32_t records;
  unsigned char *record= NULL;
  Field **field_ptr;

  local_error= 1;
  outparam.resetTable(session, this, db_stat);

  outparam.setAlias(alias);

  /* Allocate Cursor */
  if (not (outparam.cursor= db_type()->getCursor(outparam)))
    return local_error;

  local_error= 4;
  records= 0;
  if ((db_stat & HA_OPEN_KEYFILE))
    records=1;

  records++;

  record= outparam.alloc(rec_buff_length * records);

  if (records == 0)
  {
    /* We are probably in hard repair, and the buffers should not be used */
    outparam.record[0]= outparam.record[1]= getDefaultValues();
  }
  else
  {
    outparam.record[0]= record;
    if (records > 1)
      outparam.record[1]= record+ rec_buff_length;
    else
      outparam.record[1]= outparam.getInsertRecord();   // Safety
  }

#ifdef HAVE_VALGRIND
  /*
    We need this because when we read var-length rows, we are not updating
    bytes after end of varchar
  */
  if (records > 1)
  {
    memcpy(outparam.getInsertRecord(), getDefaultValues(), rec_buff_length);
    memcpy(outparam.getUpdateRecord(), getDefaultValues(), null_bytes);
    if (records > 2)
      memcpy(outparam.getUpdateRecord(), getDefaultValues(), rec_buff_length);
  }
#endif
  if (records > 1)
  {
    memcpy(outparam.getUpdateRecord(), getDefaultValues(), null_bytes);
  }

  field_ptr = new (outparam.mem()) Field*[_field_size + 1];

  outparam.setFields(field_ptr);

  record= outparam.getInsertRecord()-1;	/* Fieldstart = 1 */

  outparam.null_flags= (unsigned char*) record+1;

  /* Setup copy of fields from share, but use the right alias and record */
  for (uint32_t i= 0 ; i < _field_size; i++, field_ptr++)
  {
    if (!((*field_ptr)= _fields[i]->clone(&outparam.mem(), &outparam)))
      return local_error;
  }
  *field_ptr= 0;                              // End marker

  if (found_next_number_field)
    outparam.found_next_number_field=
      outparam.getField(positionFields(found_next_number_field));
  if (timestamp_field)
    outparam.timestamp_field= (field::Epoch*) outparam.getField(timestamp_field->position());

  /* Fix key->name and key_part->field */
  if (key_parts)
  {
    KeyInfo	*local_key_info, *key_info_end;
    KeyPartInfo *key_part;
    uint32_t n_length;
    n_length= keys*sizeof(KeyInfo) + key_parts*sizeof(KeyPartInfo);
    local_key_info= (KeyInfo*) outparam.alloc(n_length);
    outparam.key_info= local_key_info;
    key_part= (reinterpret_cast<KeyPartInfo*> (local_key_info+keys));

    memcpy(local_key_info, key_info, sizeof(*local_key_info)*keys);
    memcpy(key_part, key_info[0].key_part, (sizeof(*key_part) *
                                            key_parts));

    for (key_info_end= local_key_info + keys ;
         local_key_info < key_info_end ;
         local_key_info++)
    {
      KeyPartInfo *key_part_end;

      local_key_info->table= &outparam;
      local_key_info->key_part= key_part;

      for (key_part_end= key_part+ local_key_info->key_parts ;
           key_part < key_part_end ;
           key_part++)
      {
        Field *local_field= key_part->field= outparam.getField(key_part->fieldnr-1);

        if (local_field->key_length() != key_part->length &&
            !(local_field->flags & BLOB_FLAG))
        {
          /*
            We are using only a prefix of the column as a key:
            Create a new field for the key part that matches the index
          */
          local_field= key_part->field= local_field->new_field(&outparam.mem(), &outparam, 0);
          local_field->field_length= key_part->length;
        }
      }
    }
  }

  /* Allocate bitmaps */

  outparam.def_read_set.resize(_field_size);
  outparam.def_write_set.resize(_field_size);
  outparam.tmp_set.resize(_field_size);
  outparam.default_column_bitmaps();

  return 0;
}

int TableShare::open_table_cursor_inner(const identifier::Table &identifier,
                                        uint32_t db_stat, uint32_t ha_open_flags,
                                        Table &outparam,
                                        bool &error_reported)
{
  /* The table struct is now initialized;  Open the table */
  int local_error= 2;
  if (db_stat)
  {
    assert(!(db_stat & HA_WAIT_IF_LOCKED));
    int ha_err;

    if ((ha_err= (outparam.cursor->ha_open(identifier,
                                           (db_stat & HA_READ_ONLY ? O_RDONLY : O_RDWR),
                                           (db_stat & HA_OPEN_TEMPORARY ? HA_OPEN_TMP_TABLE : HA_OPEN_IGNORE_IF_LOCKED) | ha_open_flags))))
    {
      switch (ha_err)
      {
      case HA_ERR_NO_SUCH_TABLE:
        /*
          The table did not exists in storage engine, use same error message
          as if the .frm cursor didn't exist
        */
        local_error= 1;
        errno= ENOENT;
        break;
      case EMFILE:
        /*
          Too many files opened, use same error message as if the .frm
          cursor can't open
        */
        local_error= 1;
        errno= EMFILE;
        break;
      default:
        outparam.print_error(ha_err, MYF(0));
        error_reported= true;
        if (ha_err == HA_ERR_TABLE_DEF_CHANGED)
          local_error= 7;
        break;
      }
      return local_error;
    }
  }

  return 0;
}

/* error message when opening a form cursor */
void TableShare::open_table_error(int pass_error, int db_errno, int pass_errarg)
{
  char buff[FN_REFLEN];
  myf errortype= ME_ERROR+ME_WAITTANG;

  switch (pass_error) {
  case 7:
  case 1:
    if (db_errno == ENOENT)
    {
      identifier::Table identifier(db.str, table_name.str);
      my_error(ER_TABLE_UNKNOWN, identifier);
    }
    else
    {
      snprintf(buff, sizeof(buff), "%s",normalized_path.str);
      my_error((db_errno == EMFILE) ? ER_CANT_OPEN_FILE : ER_FILE_NOT_FOUND,
               errortype, buff, db_errno);
    }
    break;
  case 2:
    {
      drizzled::error_t err_no;

      err_no= (db_errno == ENOENT) ? ER_FILE_NOT_FOUND : (db_errno == EAGAIN) ?
        ER_FILE_USED : ER_CANT_OPEN_FILE;

      my_error(err_no, errortype, normalized_path.str, db_errno);
      break;
    }
  case 5:
    {
      const char *csname= get_charset_name((uint32_t) pass_errarg);
      char tmp[10];
      if (!csname || csname[0] =='?')
      {
        snprintf(tmp, sizeof(tmp), "#%d", pass_errarg);
        csname= tmp;
      }
      my_printf_error(ER_UNKNOWN_COLLATION,
                      _("Unknown collation '%s' in table '%-.64s' definition"),
                      MYF(0), csname, table_name.str);
      break;
    }
  case 6:
    snprintf(buff, sizeof(buff), "%s", normalized_path.str);
    my_printf_error(ER_NOT_FORM_FILE,
                    _("Table '%-.64s' was created with a different version "
                      "of Drizzle and cannot be read"),
                    MYF(0), buff);
    break;
  case 8:
    break;
  default:				/* Better wrong error than none */
  case 4:
    snprintf(buff, sizeof(buff), "%s", normalized_path.str);
    my_error(ER_NOT_FORM_FILE, errortype, buff, 0);
    break;
  }
  return;
} /* open_table_error */

Field *TableShare::make_field(const message::Table::Field &pfield,
                              unsigned char *ptr,
                              uint32_t field_length,
                              bool is_nullable,
                              unsigned char *null_pos,
                              unsigned char null_bit,
                              uint8_t decimals,
                              enum_field_types field_type,
                              const charset_info_st * field_charset,
                              Field::utype unireg_check,
                              TYPELIB *interval,
                              const char *field_name)
{
  return make_field(pfield,
                    ptr,
                    field_length,
                    is_nullable,
                    null_pos,
                    null_bit,
                    decimals,
                    field_type,
                    field_charset,
                    unireg_check,
                    interval,
                    field_name,
                    pfield.constraints().is_unsigned());
}

Field *TableShare::make_field(const message::Table::Field &,
                              unsigned char *ptr,
                              uint32_t field_length,
                              bool is_nullable,
                              unsigned char *null_pos,
                              unsigned char null_bit,
                              uint8_t decimals,
                              enum_field_types field_type,
                              const charset_info_st * field_charset,
                              Field::utype unireg_check,
                              TYPELIB *interval,
                              const char *field_name, 
                              bool is_unsigned)
{
  if (! is_nullable)
  {
    null_pos=0;
    null_bit=0;
  }
  else
  {
    null_bit= ((unsigned char) 1) << null_bit;
  }

  switch (field_type)
  {
  case DRIZZLE_TYPE_ENUM:
    return new (&mem_root) Field_enum(ptr,
                                      field_length,
                                      null_pos,
                                      null_bit,
                                      field_name,
                                      interval,
                                      field_charset);
  case DRIZZLE_TYPE_VARCHAR:
    setVariableWidth();
    return new (&mem_root) Field_varstring(ptr,field_length,
                                      ha_varchar_packlength(field_length),
                                      null_pos,null_bit,
                                      field_name,
                                      field_charset);
  case DRIZZLE_TYPE_BLOB:
    return new (&mem_root) Field_blob(ptr,
                                      null_pos,
                                      null_bit,
                                      field_name,
                                      this,
                                      field_charset);
  case DRIZZLE_TYPE_DECIMAL:
    return new (&mem_root) Field_decimal(ptr,
                                         field_length,
                                         null_pos,
                                         null_bit,
                                         unireg_check,
                                         field_name,
                                         decimals);
  case DRIZZLE_TYPE_DOUBLE:
    return new (&mem_root) Field_double(ptr,
                                   field_length,
                                   null_pos,
                                   null_bit,
                                   unireg_check,
                                   field_name,
                                   decimals,
                                   false,
                                   false /* is_unsigned */);
  case DRIZZLE_TYPE_UUID:
    return new (&mem_root) field::Uuid(ptr,
                                       field_length,
                                       null_pos,
                                       null_bit,
                                       field_name);
  case DRIZZLE_TYPE_BOOLEAN:
    return new (&mem_root) field::Boolean(ptr,
                                          field_length,
                                          null_pos,
                                          null_bit,
                                          field_name,
                                          is_unsigned);
  case DRIZZLE_TYPE_LONG:
    return new (&mem_root) field::Int32(ptr,
                                        field_length,
                                        null_pos,
                                        null_bit,
                                        unireg_check,
                                        field_name);
  case DRIZZLE_TYPE_LONGLONG:
    {
      if (is_unsigned)
      {
        return new (&mem_root) field::Size(ptr,
                                           field_length,
                                           null_pos,
                                           null_bit,
                                           unireg_check,
                                           field_name);
      }

      return new (&mem_root) field::Int64(ptr,
                                          field_length,
                                          null_pos,
                                          null_bit,
                                          unireg_check,
                                          field_name);
    }
  case DRIZZLE_TYPE_MICROTIME:
    return new (&mem_root) field::Microtime(ptr,
                                            null_pos,
                                            null_bit,
                                            unireg_check,
                                            field_name,
                                            this);
  case DRIZZLE_TYPE_TIMESTAMP:
    return new (&mem_root) field::Epoch(ptr,
                                        null_pos,
                                        null_bit,
                                        unireg_check,
                                        field_name,
                                        this);
  case DRIZZLE_TYPE_TIME:
    return new (&mem_root) field::Time(ptr,
                                       field_length,
                                       null_pos,
                                       null_bit,
                                       field_name);
  case DRIZZLE_TYPE_DATE:
    return new (&mem_root) Field_date(ptr,
                                 null_pos,
                                 null_bit,
                                 field_name);
  case DRIZZLE_TYPE_DATETIME:
    return new (&mem_root) Field_datetime(ptr,
                                     null_pos,
                                     null_bit,
                                     field_name);
  case DRIZZLE_TYPE_NULL:
    return new (&mem_root) Field_null(ptr,
                                      field_length,
                                      field_name);
  }
  assert(0);
  abort();
}

void TableShare::refreshVersion()
{
  version= g_refresh_version;
}


} /* namespace drizzled */
