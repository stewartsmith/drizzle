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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* Some general useful functions */

#include "config.h"

#include <float.h>
#include <fcntl.h>

#include <string>
#include <vector>
#include <algorithm>

#include <drizzled/error.h>
#include <drizzled/gettext.h>

#include "drizzled/plugin/transactional_storage_engine.h"
#include "drizzled/plugin/authorization.h"
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
#include <drizzled/unireg.h>
#include <drizzled/message/table.pb.h>
#include "drizzled/sql_table.h"
#include "drizzled/charset.h"
#include "drizzled/internal/m_string.h"
#include "plugin/myisam/myisam.h"

#include <drizzled/item/string.h>
#include <drizzled/item/int.h>
#include <drizzled/item/decimal.h>
#include <drizzled/item/float.h>
#include <drizzled/item/null.h>
#include <drizzled/temporal.h>

#include "drizzled/table_proto.h"

using namespace std;

namespace drizzled
{

extern pid_t current_pid;
extern plugin::StorageEngine *heap_engine;
extern plugin::StorageEngine *myisam_engine;

/* Functions defined in this cursor */

void open_table_error(TableShare *share, int error, int db_errno,
                      myf errortype, int errarg);

/*************************************************************************/

/* Get column name from column hash */

static unsigned char *get_field_name(Field **buff, size_t *length, bool)
{
  *length= (uint32_t) strlen((*buff)->field_name);
  return (unsigned char*) (*buff)->field_name;
}

/*
  Allocate a setup TableShare structure

  SYNOPSIS
    alloc_table_share()
    TableList		Take database and table name from there
    key			Table cache key (db \0 table_name \0...)
    key_length		Length of key

  RETURN
    0  Error (out of memory)
    #  Share
*/

TableShare *alloc_table_share(TableList *table_list, char *key,
                               uint32_t key_length)
{
  memory::Root mem_root;
  TableShare *share;
  char *key_buff, *path_buff;
  std::string path;

  build_table_filename(path, table_list->db, table_list->table_name, false);

  memory::init_sql_alloc(&mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  if (multi_alloc_root(&mem_root,
                       &share, sizeof(*share),
                       &key_buff, key_length,
                       &path_buff, path.length() + 1,
                       NULL))
  {
    memset(share, 0, sizeof(*share));

    share->set_table_cache_key(key_buff, key, key_length);

    share->path.str= path_buff,
    share->path.length= path.length();
    strcpy(share->path.str, path.c_str());
    share->normalized_path.str=    share->path.str;
    share->normalized_path.length= path.length();

    share->version=       refresh_version;

    memcpy(&share->mem_root, &mem_root, sizeof(mem_root));
    pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
    pthread_cond_init(&share->cond, NULL);
  }
  return(share);
}


static enum_field_types proto_field_type_to_drizzle_type(uint32_t proto_field_type)
{
  enum_field_types field_type;

  switch(proto_field_type)
  {
  case message::Table::Field::INTEGER:
    field_type= DRIZZLE_TYPE_LONG;
    break;
  case message::Table::Field::DOUBLE:
    field_type= DRIZZLE_TYPE_DOUBLE;
    break;
  case message::Table::Field::TIMESTAMP:
    field_type= DRIZZLE_TYPE_TIMESTAMP;
    break;
  case message::Table::Field::BIGINT:
    field_type= DRIZZLE_TYPE_LONGLONG;
    break;
  case message::Table::Field::DATETIME:
    field_type= DRIZZLE_TYPE_DATETIME;
    break;
  case message::Table::Field::DATE:
    field_type= DRIZZLE_TYPE_DATE;
    break;
  case message::Table::Field::VARCHAR:
    field_type= DRIZZLE_TYPE_VARCHAR;
    break;
  case message::Table::Field::DECIMAL:
    field_type= DRIZZLE_TYPE_DECIMAL;
    break;
  case message::Table::Field::ENUM:
    field_type= DRIZZLE_TYPE_ENUM;
    break;
  case message::Table::Field::BLOB:
    field_type= DRIZZLE_TYPE_BLOB;
    break;
  default:
    field_type= DRIZZLE_TYPE_LONG; /* Set value to kill GCC warning */
    assert(1);
  }

  return field_type;
}

static Item *default_value_item(enum_field_types field_type,
	                        const CHARSET_INFO *charset,
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
    break;
  case DRIZZLE_TYPE_DOUBLE:
    default_item= new Item_float(default_value->c_str(),
				 default_value->length());
    break;
  case DRIZZLE_TYPE_NULL:
    assert(false);
  case DRIZZLE_TYPE_TIMESTAMP:
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_DATE:
    if (default_value->compare("NOW()") == 0)
      break;
  case DRIZZLE_TYPE_ENUM:
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

int parse_table_proto(Session& session,
                      message::Table &table,
                      TableShare *share)
{
  int error= 0;

  if (! table.IsInitialized())
  {
    my_error(ER_CORRUPT_TABLE_DEFINITION, MYF(0), table.InitializationErrorString().c_str());
    return ER_CORRUPT_TABLE_DEFINITION;
  }

  share->setTableProto(new(nothrow) message::Table(table));

  share->storage_engine= plugin::StorageEngine::findByName(session, table.engine().name());
  assert(share->storage_engine); // We use an assert() here because we should never get this far and still have no suitable engine.

  message::Table::TableOptions table_options;

  if (table.has_options())
    table_options= table.options();

  uint32_t db_create_options= 0;

  if (table_options.has_pack_keys())
  {
    if (table_options.pack_keys())
      db_create_options|= HA_OPTION_PACK_KEYS;
    else
      db_create_options|= HA_OPTION_NO_PACK_KEYS;
  }

  if (table_options.pack_record())
    db_create_options|= HA_OPTION_PACK_RECORD;

  /* db_create_options was stored as 2 bytes in FRM
     Any HA_OPTION_ that doesn't fit into 2 bytes was silently truncated away.
   */
  share->db_create_options= (db_create_options & 0x0000FFFF);
  share->db_options_in_use= share->db_create_options;

  share->row_type= table_options.has_row_type() ?
    (enum row_type) table_options.row_type() : ROW_TYPE_DEFAULT;

  share->block_size= table_options.has_block_size() ?
    table_options.block_size() : 0;

  share->table_charset= get_charset(table_options.has_collation_id()?
				    table_options.collation_id() : 0);

  if (!share->table_charset)
  {
    /* unknown charset in head[38] or pre-3.23 frm */
    if (use_mb(default_charset_info))
    {
      /* Warn that we may be changing the size of character columns */
      errmsg_printf(ERRMSG_LVL_WARN,
		    _("'%s' had no or invalid character set, "
		      "and default character set is multi-byte, "
		      "so character column sizes may have changed"),
		    share->path.str);
    }
    share->table_charset= default_charset_info;
  }

  share->db_record_offset= 1;

  share->blob_ptr_size= portable_sizeof_char_ptr; // more bonghits.

  share->keys= table.indexes_size();

  share->key_parts= 0;
  for (int indx= 0; indx < table.indexes_size(); indx++)
    share->key_parts+= table.indexes(indx).index_part_size();

  share->key_info= (KEY*) alloc_root(&share->mem_root,
				     table.indexes_size() * sizeof(KEY)
				     +share->key_parts*sizeof(KEY_PART_INFO));

  KEY_PART_INFO *key_part;

  key_part= reinterpret_cast<KEY_PART_INFO*>
    (share->key_info+table.indexes_size());


  ulong *rec_per_key= (ulong*) alloc_root(&share->mem_root,
					    sizeof(ulong*)*share->key_parts);

  share->keynames.count= table.indexes_size();
  share->keynames.name= NULL;
  share->keynames.type_names= (const char**)
    alloc_root(&share->mem_root, sizeof(char*) * (table.indexes_size()+1));

  share->keynames.type_lengths= (unsigned int*)
    alloc_root(&share->mem_root,
	       sizeof(unsigned int) * (table.indexes_size()+1));

  share->keynames.type_names[share->keynames.count]= NULL;
  share->keynames.type_lengths[share->keynames.count]= 0;

  KEY* keyinfo= share->key_info;
  for (int keynr= 0; keynr < table.indexes_size(); keynr++, keyinfo++)
  {
    message::Table::Index indx= table.indexes(keynr);

    keyinfo->table= 0;
    keyinfo->flags= 0;

    if (indx.is_unique())
      keyinfo->flags|= HA_NOSAME;

    if (indx.has_options())
    {
      message::Table::Index::IndexOptions indx_options= indx.options();
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

      key_part->store_length= key_part->length;

      /* key_part->offset is set later */
      key_part->key_type= part.key_type();
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
      keyinfo->comment.str= strmake_root(&share->mem_root,
					 indx.comment().c_str(),
					 keyinfo->comment.length);
    }

    keyinfo->name= strmake_root(&share->mem_root,
				indx.name().c_str(),
				indx.name().length());

    share->keynames.type_names[keynr]= keyinfo->name;
    share->keynames.type_lengths[keynr]= indx.name().length();
  }

  share->keys_for_keyread.reset();
  set_prefix(share->keys_in_use, share->keys);

  share->fields= table.field_size();

  share->field= (Field**) alloc_root(&share->mem_root,
				     ((share->fields+1) * sizeof(Field*)));
  share->field[share->fields]= NULL;

  uint32_t null_fields= 0;
  share->reclength= 0;

  uint32_t *field_offsets= (uint32_t*)malloc(share->fields * sizeof(uint32_t));
  uint32_t *field_pack_length=(uint32_t*)malloc(share->fields*sizeof(uint32_t));

  assert(field_offsets && field_pack_length); // TODO: fixme

  uint32_t interval_count= 0;
  uint32_t interval_parts= 0;

  uint32_t stored_columns_reclength= 0;

  for (unsigned int fieldnr= 0; fieldnr < share->fields; fieldnr++)
  {
    message::Table::Field pfield= table.field(fieldnr);
    if (pfield.constraints().is_nullable())
      null_fields++;

    enum_field_types drizzle_field_type=
      proto_field_type_to_drizzle_type(pfield.type());

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

        const CHARSET_INFO *cs= get_charset(field_options.has_collation_id() ?
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

        field_pack_length[fieldnr]=
          get_enum_pack_length(field_options.field_value_size());

        interval_count++;
        interval_parts+= field_options.field_value_size();
      }
      break;
    case DRIZZLE_TYPE_DECIMAL:
      {
        message::Table::Field::NumericFieldOptions fo= pfield.numeric_options();

        field_pack_length[fieldnr]= my_decimal_get_binary_size(fo.precision(), fo.scale());
      }
      break;
    default:
      /* Zero is okay here as length is fixed for other types. */
      field_pack_length[fieldnr]= calc_pack_length(drizzle_field_type, 0);
    }

    share->reclength+= field_pack_length[fieldnr];
    stored_columns_reclength+= field_pack_length[fieldnr];
  }

  /* data_offset added to stored_rec_length later */
  share->stored_rec_length= stored_columns_reclength;

  share->null_fields= null_fields;

  ulong null_bits= null_fields;
  if (! table_options.pack_record())
    null_bits++;
  ulong data_offset= (null_bits + 7)/8;


  share->reclength+= data_offset;
  share->stored_rec_length+= data_offset;

  ulong rec_buff_length;

  rec_buff_length= ALIGN_SIZE(share->reclength + 1);
  share->rec_buff_length= rec_buff_length;

  unsigned char* record= NULL;

  if (! (record= (unsigned char *) alloc_root(&share->mem_root,
                                              rec_buff_length)))
    abort();

  memset(record, 0, rec_buff_length);

  int null_count= 0;

  if (! table_options.pack_record())
  {
    null_count++; // one bit for delete mark.
    *record|= 1;
  }

  share->default_values= record;

  if (interval_count)
  {
    share->intervals= (TYPELIB *) alloc_root(&share->mem_root,
                                           interval_count*sizeof(TYPELIB));
  }
  else
    share->intervals= NULL;

  share->fieldnames.type_names= (const char **) alloc_root(&share->mem_root,
                                                          (share->fields + 1) * sizeof(char*));

  share->fieldnames.type_lengths= (unsigned int *) alloc_root(&share->mem_root,
                                                             (share->fields + 1) * sizeof(unsigned int));

  share->fieldnames.type_names[share->fields]= NULL;
  share->fieldnames.type_lengths[share->fields]= 0;
  share->fieldnames.count= share->fields;


  /* Now fix the TYPELIBs for the intervals (enum values)
     and field names.
   */

  uint32_t interval_nr= 0;

  for (unsigned int fieldnr= 0; fieldnr < share->fields; fieldnr++)
  {
    message::Table::Field pfield= table.field(fieldnr);

    /* field names */
    share->fieldnames.type_names[fieldnr]= strmake_root(&share->mem_root,
                                                        pfield.name().c_str(),
                                                        pfield.name().length());

    share->fieldnames.type_lengths[fieldnr]= pfield.name().length();

    /* enum typelibs */
    if (pfield.type() != message::Table::Field::ENUM)
      continue;

    message::Table::Field::EnumerationValues field_options= pfield.enumeration_values();

    const CHARSET_INFO *charset= get_charset(field_options.has_collation_id() ?
                                             field_options.collation_id() : 0);

    if (! charset)
      charset= default_charset_info;

    TYPELIB *t= &(share->intervals[interval_nr]);

    t->type_names= (const char**)alloc_root(&share->mem_root,
                                            (field_options.field_value_size() + 1) * sizeof(char*));

    t->type_lengths= (unsigned int*) alloc_root(&share->mem_root,
                                                (field_options.field_value_size() + 1) * sizeof(unsigned int));

    t->type_names[field_options.field_value_size()]= NULL;
    t->type_lengths[field_options.field_value_size()]= 0;

    t->count= field_options.field_value_size();
    t->name= NULL;

    for (int n= 0; n < field_options.field_value_size(); n++)
    {
      t->type_names[n]= strmake_root(&share->mem_root,
                                     field_options.field_value(n).c_str(),
                                     field_options.field_value(n).length());

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

  bool use_hash= share->fields >= MAX_FIELDS_BEFORE_HASH;

  if (use_hash)
    use_hash= ! hash_init(&share->name_hash,
                          system_charset_info,
                          share->fields,
                          0,
                          0,
                          (hash_get_key) get_field_name,
                          0,
                          0);

  unsigned char* null_pos= record;;
  int null_bit_pos= (table_options.pack_record()) ? 0 : 1;

  for (unsigned int fieldnr= 0; fieldnr < share->fields; fieldnr++)
  {
    message::Table::Field pfield= table.field(fieldnr);

    enum column_format_type column_format= COLUMN_FORMAT_TYPE_DEFAULT;

    switch (pfield.format())
    {
    case message::Table::Field::DefaultFormat:
      column_format= COLUMN_FORMAT_TYPE_DEFAULT;
      break;
    case message::Table::Field::FixedFormat:
      column_format= COLUMN_FORMAT_TYPE_FIXED;
      break;
    case message::Table::Field::DynamicFormat:
      column_format= COLUMN_FORMAT_TYPE_DYNAMIC;
      break;
    default:
      assert(1);
    }

    Field::utype unireg_type= Field::NONE;

    if (pfield.has_numeric_options() &&
        pfield.numeric_options().is_autoincrement())
    {
      unireg_type= Field::NEXT_NUMBER;
    }

    if (pfield.has_options() &&
        pfield.options().has_default_value() &&
        pfield.options().default_value().compare("NOW()") == 0)
    {
      if (pfield.options().has_update_value() &&
          pfield.options().update_value().compare("NOW()") == 0)
      {
        unireg_type= Field::TIMESTAMP_DNUN_FIELD;
      }
      else if (! pfield.options().has_update_value())
      {
      	unireg_type= Field::TIMESTAMP_DN_FIELD;
      }
      else
      	assert(1); // Invalid update value.
    }
    else if (pfield.has_options() &&
             pfield.options().has_update_value() &&
             pfield.options().update_value().compare("NOW()") == 0)
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

      comment.str= strmake_root(&share->mem_root, str, len);
      comment.length= len;
    }

    enum_field_types field_type;

    field_type= proto_field_type_to_drizzle_type(pfield.type());

    const CHARSET_INFO *charset= &my_charset_bin;

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
          error= 4;
          goto err;
        }
        decimals= static_cast<uint8_t>(fo.scale());
      }
    }

    Item *default_value= NULL;

    if (pfield.options().has_default_value() ||
        pfield.options().has_default_null()  ||
        pfield.options().has_default_bin_value())
    {
      default_value= default_value_item(field_type,
                                        charset,
                                        pfield.options().default_null(),
                                        &pfield.options().default_value(),
                                        &pfield.options().default_bin_value());
    }


    Table temp_table; /* Use this so that BLOB DEFAULT '' works */
    memset(&temp_table, 0, sizeof(temp_table));
    temp_table.s= share;
    temp_table.in_use= &session;
    temp_table.s->db_low_byte_first= true; //Cursor->low_byte_first();
    temp_table.s->blob_ptr_size= portable_sizeof_char_ptr;

    uint32_t field_length= 0; //Assignment is for compiler complaint.

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
        error= 1;
        goto err;
      }
      break;
    }
    case DRIZZLE_TYPE_DECIMAL:
    {
      message::Table::Field::NumericFieldOptions fo= pfield.numeric_options();

      field_length= my_decimal_precision_to_length(fo.precision(), fo.scale(),
                                                   false);
      break;
    }
    case DRIZZLE_TYPE_TIMESTAMP:
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
      field_length= MAX_BIGINT_WIDTH;
      break;
    case DRIZZLE_TYPE_NULL:
      abort(); // Programming error
    }

    Field* f= make_field(share,
                         &share->mem_root,
                         record + field_offsets[fieldnr] + data_offset,
                         field_length,
                         pfield.constraints().is_nullable(),
                         null_pos,
                         null_bit_pos,
                         decimals,
                         field_type,
                         charset,
                         (Field::utype) MTYP_TYPENR(unireg_type),
                         ((field_type == DRIZZLE_TYPE_ENUM) ?
                          share->intervals + (interval_nr++)
                          : (TYPELIB*) 0),
                         share->fieldnames.type_names[fieldnr]);

    share->field[fieldnr]= f;

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
      session.count_cuted_fields= CHECK_FIELD_WARN;
      int res= default_value->save_in_field(f, 1);
      session.count_cuted_fields= old_count_cuted_fields;
      if (res != 0 && res != 3) /* @TODO Huh? */
      {
        my_error(ER_INVALID_DEFAULT, MYF(0), f->field_name);
        error= 1;
        goto err;
      }
    }
    else if (f->real_type() == DRIZZLE_TYPE_ENUM &&
             (f->flags & NOT_NULL_FLAG))
    {
      f->set_notnull();
      f->store((int64_t) 1, true);
    }
    else
      f->reset();

    /* hack to undo f->init() */
    f->table= NULL;
    f->orig_table= NULL;

    f->field_index= fieldnr;
    f->comment= comment;
    if (! default_value &&
        ! (f->unireg_check==Field::NEXT_NUMBER) &&
        (f->flags & NOT_NULL_FLAG) &&
        (f->real_type() != DRIZZLE_TYPE_TIMESTAMP))
    {
      f->flags|= NO_DEFAULT_VALUE_FLAG;
    }

    if (f->unireg_check == Field::NEXT_NUMBER)
      share->found_next_number_field= &(share->field[fieldnr]);

    if (share->timestamp_field == f)
      share->timestamp_field_offset= fieldnr;

    if (use_hash) /* supposedly this never fails... but comments lie */
      (void) my_hash_insert(&share->name_hash,
			    (unsigned char*)&(share->field[fieldnr]));

  }

  keyinfo= share->key_info;
  for (unsigned int keynr= 0; keynr < share->keys; keynr++, keyinfo++)
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

  share->null_bytes= (null_pos - (unsigned char*) record + (null_bit_pos + 7) / 8);

  share->last_null_bit_pos= null_bit_pos;

  free(field_offsets);
  field_offsets= NULL;
  free(field_pack_length);
  field_pack_length= NULL;

  /* Fix key stuff */
  if (share->key_parts)
  {
    uint32_t primary_key= (uint32_t) (find_type((char*) "PRIMARY",
                                                &share->keynames, 3) - 1); /* @TODO Huh? */

    keyinfo= share->key_info;
    key_part= keyinfo->key_part;

    for (uint32_t key= 0; key < share->keys; key++,keyinfo++)
    {
      uint32_t usable_parts= 0;

      if (primary_key >= MAX_KEY && (keyinfo->flags & HA_NOSAME))
      {
        /*
          If the UNIQUE key doesn't have NULL columns and is not a part key
          declare this as a primary key.
        */
        primary_key=key;
        for (uint32_t i= 0; i < keyinfo->key_parts; i++)
        {
          uint32_t fieldnr= key_part[i].fieldnr;
          if (! fieldnr ||
              share->field[fieldnr-1]->null_ptr ||
              share->field[fieldnr-1]->key_length() != key_part[i].length)
          {
            primary_key= MAX_KEY; // Can't be used
            break;
          }
        }
      }

      for (uint32_t i= 0 ; i < keyinfo->key_parts ; key_part++,i++)
      {
        Field *field;
        if (! key_part->fieldnr)
        {
          abort(); // goto err;
        }
        field= key_part->field= share->field[key_part->fieldnr-1];
        key_part->type= field->key_type();
        if (field->null_ptr)
        {
          key_part->null_offset=(uint32_t) ((unsigned char*) field->null_ptr -
                                        share->default_values);
          key_part->null_bit= field->null_bit;
          key_part->store_length+=HA_KEY_NULL_LENGTH;
          keyinfo->flags|=HA_NULL_PART_KEY;
          keyinfo->extra_length+= HA_KEY_NULL_LENGTH;
          keyinfo->key_length+= HA_KEY_NULL_LENGTH;
        }
        if (field->type() == DRIZZLE_TYPE_BLOB ||
            field->real_type() == DRIZZLE_TYPE_VARCHAR)
        {
          if (field->type() == DRIZZLE_TYPE_BLOB)
            key_part->key_part_flag|= HA_BLOB_PART;
          else
            key_part->key_part_flag|= HA_VAR_LENGTH_PART;
          keyinfo->extra_length+=HA_KEY_BLOB_LENGTH;
          key_part->store_length+=HA_KEY_BLOB_LENGTH;
          keyinfo->key_length+= HA_KEY_BLOB_LENGTH;
        }
        if (i == 0 && key != primary_key)
          field->flags |= (((keyinfo->flags & HA_NOSAME) &&
                           (keyinfo->key_parts == 1)) ?
                           UNIQUE_KEY_FLAG : MULTIPLE_KEY_FLAG);
        if (i == 0)
          field->key_start.set(key);
        if (field->key_length() == key_part->length &&
            !(field->flags & BLOB_FLAG))
        {
          enum ha_key_alg algo= share->key_info[key].algorithm;
          if (share->db_type()->index_flags(algo) & HA_KEYREAD_ONLY)
          {
            share->keys_for_keyread.set(key);
            field->part_of_key.set(key);
            field->part_of_key_not_clustered.set(key);
          }
          if (share->db_type()->index_flags(algo) & HA_READ_ORDER)
            field->part_of_sortkey.set(key);
        }
        if (!(key_part->key_part_flag & HA_REVERSE_SORT) &&
            usable_parts == i)
          usable_parts++;			// For FILESORT
        field->flags|= PART_KEY_FLAG;
        if (key == primary_key)
        {
          field->flags|= PRI_KEY_FLAG;
          /*
            If this field is part of the primary key and all keys contains
            the primary key, then we can use any key to find this column
          */
          if (share->storage_engine->check_flag(HTON_BIT_PRIMARY_KEY_IN_READ_INDEX))
          {
            field->part_of_key= share->keys_in_use;
            if (field->part_of_sortkey.test(key))
              field->part_of_sortkey= share->keys_in_use;
          }
        }
        if (field->key_length() != key_part->length)
        {
          key_part->key_part_flag|= HA_PART_KEY_SEG;
        }
      }
      keyinfo->usable_key_parts= usable_parts; // Filesort

      set_if_bigger(share->max_key_length,keyinfo->key_length+
                    keyinfo->key_parts);
      share->total_key_length+= keyinfo->key_length;

      if (keyinfo->flags & HA_NOSAME)
      {
        set_if_bigger(share->max_unique_length,keyinfo->key_length);
      }
    }
    if (primary_key < MAX_KEY &&
        (share->keys_in_use.test(primary_key)))
    {
      share->primary_key= primary_key;
      /*
        If we are using an integer as the primary key then allow the user to
        refer to it as '_rowid'
      */
      if (share->key_info[primary_key].key_parts == 1)
      {
        Field *field= share->key_info[primary_key].key_part[0].field;
        if (field && field->result_type() == INT_RESULT)
        {
          /* note that fieldnr here (and rowid_field_offset) starts from 1 */
          share->rowid_field_offset= (share->key_info[primary_key].key_part[0].
                                      fieldnr);
        }
      }
    }
    else
      share->primary_key = MAX_KEY; // we do not have a primary key
  }
  else
    share->primary_key= MAX_KEY;

  if (share->found_next_number_field)
  {
    Field *reg_field= *share->found_next_number_field;
    if ((int) (share->next_number_index= (uint32_t)
	       find_ref_key(share->key_info, share->keys,
                            share->default_values, reg_field,
			    &share->next_number_key_offset,
                            &share->next_number_keypart)) < 0)
    {
      /* Wrong field definition */
      error= 4;
      goto err;
    }
    else
      reg_field->flags |= AUTO_INCREMENT_FLAG;
  }

  if (share->blob_fields)
  {
    Field **ptr;
    uint32_t k, *save;

    /* Store offsets to blob fields to find them fast */
    if (!(share->blob_field= save=
	  (uint*) alloc_root(&share->mem_root,
                             (uint32_t) (share->blob_fields* sizeof(uint32_t)))))
      goto err;
    for (k= 0, ptr= share->field ; *ptr ; ptr++, k++)
    {
      if ((*ptr)->flags & BLOB_FLAG)
        (*save++)= k;
    }
  }

  share->db_low_byte_first= true; // @todo Question this.
  share->column_bitmap_size= bitmap_buffer_size(share->fields);

  my_bitmap_map *bitmaps;

  if (!(bitmaps= (my_bitmap_map*) alloc_root(&share->mem_root,
                                             share->column_bitmap_size)))
    goto err;
  share->all_set.init(bitmaps, share->fields);
  share->all_set.setAll();

  return (0);

err:
  if (field_offsets)
    free(field_offsets);
  if (field_pack_length)
    free(field_pack_length);

  share->error= error;
  share->open_errno= errno;
  share->errarg= 0;
  hash_free(&share->name_hash);
  share->open_table_error(error, share->open_errno, 0);

  return error;
}

/*
  Read table definition from a binary / text based .frm cursor

  SYNOPSIS
  open_table_def()
  session		Thread Cursor
  share		Fill this with table definition

  NOTES
    This function is called when the table definition is not cached in
    table_def_cache
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

int open_table_def(Session& session, TableIdentifier &identifier, TableShare *share)
{
  int error;
  bool error_given;

  error= 1;
  error_given= 0;

  message::Table table;

  error= plugin::StorageEngine::getTableDefinition(session, identifier, table);

  if (error != EEXIST)
  {
    if (error > 0)
    {
      errno= error;
      error= 1;
    }
    else
    {
      if (not table.IsInitialized())
      {
	error= 4;
      }
    }
    goto err_not_open;
  }

  error= parse_table_proto(session, table, share);

  share->table_category= TABLE_CATEGORY_USER;

err_not_open:
  if (error && !error_given)
  {
    share->error= error;
    share->open_table_error(error, (share->open_errno= errno), 0);
  }

  return(error);
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

int open_table_from_share(Session *session, TableShare *share, const char *alias,
                          uint32_t db_stat, uint32_t ha_open_flags,
                          Table *outparam)
{
  int error;
  uint32_t records, i, bitmap_size;
  bool error_reported= false;
  unsigned char *record, *bitmaps;
  Field **field_ptr;

  /* Parsing of partitioning information from .frm needs session->lex set up. */
  assert(session->lex->is_lex_started);

  error= 1;
  outparam->resetTable(session, share, db_stat);


  if (not (outparam->alias= strdup(alias)))
    goto err;

  /* Allocate Cursor */
  if (not (outparam->cursor= share->db_type()->getCursor(*share, &outparam->mem_root)))
    goto err;

  error= 4;
  records= 0;
  if ((db_stat & HA_OPEN_KEYFILE))
    records=1;

  records++;

  if (!(record= (unsigned char*) alloc_root(&outparam->mem_root,
                                   share->rec_buff_length * records)))
    goto err;

  if (records == 0)
  {
    /* We are probably in hard repair, and the buffers should not be used */
    outparam->record[0]= outparam->record[1]= share->default_values;
  }
  else
  {
    outparam->record[0]= record;
    if (records > 1)
      outparam->record[1]= record+ share->rec_buff_length;
    else
      outparam->record[1]= outparam->record[0];   // Safety
  }

#ifdef HAVE_purify
  /*
    We need this because when we read var-length rows, we are not updating
    bytes after end of varchar
  */
  if (records > 1)
  {
    memcpy(outparam->record[0], share->default_values, share->rec_buff_length);
    memcpy(outparam->record[1], share->default_values, share->null_bytes);
    if (records > 2)
      memcpy(outparam->record[1], share->default_values,
             share->rec_buff_length);
  }
#endif

  if (!(field_ptr = (Field **) alloc_root(&outparam->mem_root,
                                          (uint32_t) ((share->fields+1)*
                                                  sizeof(Field*)))))
    goto err;

  outparam->field= field_ptr;

  record= (unsigned char*) outparam->record[0]-1;	/* Fieldstart = 1 */

  outparam->null_flags= (unsigned char*) record+1;

  /* Setup copy of fields from share, but use the right alias and record */
  for (i= 0 ; i < share->fields; i++, field_ptr++)
  {
    if (!((*field_ptr)= share->field[i]->clone(&outparam->mem_root, outparam)))
      goto err;
  }
  (*field_ptr)= 0;                              // End marker

  if (share->found_next_number_field)
    outparam->found_next_number_field=
      outparam->field[(uint32_t) (share->found_next_number_field - share->field)];
  if (share->timestamp_field)
    outparam->timestamp_field= (Field_timestamp*) outparam->field[share->timestamp_field_offset];


  /* Fix key->name and key_part->field */
  if (share->key_parts)
  {
    KEY	*key_info, *key_info_end;
    KEY_PART_INFO *key_part;
    uint32_t n_length;
    n_length= share->keys*sizeof(KEY) + share->key_parts*sizeof(KEY_PART_INFO);
    if (!(key_info= (KEY*) alloc_root(&outparam->mem_root, n_length)))
      goto err;
    outparam->key_info= key_info;
    key_part= (reinterpret_cast<KEY_PART_INFO*> (key_info+share->keys));

    memcpy(key_info, share->key_info, sizeof(*key_info)*share->keys);
    memcpy(key_part, share->key_info[0].key_part, (sizeof(*key_part) *
                                                   share->key_parts));

    for (key_info_end= key_info + share->keys ;
         key_info < key_info_end ;
         key_info++)
    {
      KEY_PART_INFO *key_part_end;

      key_info->table= outparam;
      key_info->key_part= key_part;

      for (key_part_end= key_part+ key_info->key_parts ;
           key_part < key_part_end ;
           key_part++)
      {
        Field *field= key_part->field= outparam->field[key_part->fieldnr-1];

        if (field->key_length() != key_part->length &&
            !(field->flags & BLOB_FLAG))
        {
          /*
            We are using only a prefix of the column as a key:
            Create a new field for the key part that matches the index
          */
          field= key_part->field=field->new_field(&outparam->mem_root,
                                                  outparam, 0);
          field->field_length= key_part->length;
        }
      }
    }
  }

  /* Allocate bitmaps */

  bitmap_size= share->column_bitmap_size;
  if (!(bitmaps= (unsigned char*) alloc_root(&outparam->mem_root, bitmap_size*3)))
    goto err;
  outparam->def_read_set.init((my_bitmap_map*) bitmaps, share->fields);
  outparam->def_write_set.init((my_bitmap_map*) (bitmaps+bitmap_size), share->fields);
  outparam->tmp_set.init((my_bitmap_map*) (bitmaps+bitmap_size*2), share->fields);
  outparam->default_column_bitmaps();

  /* The table struct is now initialized;  Open the table */
  error= 2;
  if (db_stat)
  {
    int ha_err;
    if ((ha_err= (outparam->cursor->
                  ha_open(outparam, share->normalized_path.str,
                          (db_stat & HA_READ_ONLY ? O_RDONLY : O_RDWR),
                          (db_stat & HA_OPEN_TEMPORARY ? HA_OPEN_TMP_TABLE :
                           (db_stat & HA_WAIT_IF_LOCKED) ?  HA_OPEN_WAIT_IF_LOCKED :
                           (db_stat & (HA_ABORT_IF_LOCKED | HA_GET_INFO)) ?
                          HA_OPEN_ABORT_IF_LOCKED :
                           HA_OPEN_IGNORE_IF_LOCKED) | ha_open_flags))))
    {
      switch (ha_err)
      {
        case HA_ERR_NO_SUCH_TABLE:
	  /*
            The table did not exists in storage engine, use same error message
            as if the .frm cursor didn't exist
          */
	  error= 1;
	  errno= ENOENT;
          break;
        case EMFILE:
	  /*
            Too many files opened, use same error message as if the .frm
            cursor can't open
           */
	  error= 1;
	  errno= EMFILE;
          break;
        default:
          outparam->print_error(ha_err, MYF(0));
          error_reported= true;
          if (ha_err == HA_ERR_TABLE_DEF_CHANGED)
            error= 7;
          break;
      }
      goto err;
    }
  }

#if defined(HAVE_purify)
  memset(bitmaps, 0, bitmap_size*3);
#endif

  return 0;

 err:
  if (!error_reported)
    share->open_table_error(error, errno, 0);
  delete outparam->cursor;
  outparam->cursor= 0;				// For easier error checking
  outparam->db_stat= 0;
  free_root(&outparam->mem_root, MYF(0));       // Safe to call on zeroed root
  free((char*) outparam->alias);
  return (error);
}

bool Table::fill_item_list(List<Item> *item_list) const
{
  /*
    All Item_field's created using a direct pointer to a field
    are fixed in Item_field constructor.
  */
  for (Field **ptr= field; *ptr; ptr++)
  {
    Item_field *item= new Item_field(*ptr);
    if (!item || item_list->push_back(item))
      return true;
  }
  return false;
}

int Table::closefrm(bool free_share)
{
  int error= 0;

  if (db_stat)
    error= cursor->close();
  free((char*) alias);
  alias= NULL;
  if (field)
  {
    for (Field **ptr=field ; *ptr ; ptr++)
      delete *ptr;
    field= 0;
  }
  delete cursor;
  cursor= 0;				/* For easier errorchecking */
  if (free_share)
  {
    if (s->tmp_table == message::Table::STANDARD)
      TableShare::release(s);
    else
      s->free_table_share();
  }
  free_root(&mem_root, MYF(0));

  return error;
}


void Table::resetTable(Session *session,
                       TableShare *share,
                       uint32_t db_stat_arg)
{
  s= share;
  field= NULL;

  cursor= NULL;
  next= NULL;
  prev= NULL;

  read_set= NULL;
  write_set= NULL;

  tablenr= 0;
  db_stat= db_stat_arg;

  in_use= session;
  record[0]= (unsigned char *) NULL;
  record[1]= (unsigned char *) NULL;

  insert_values= NULL;
  key_info= NULL;
  next_number_field= NULL;
  found_next_number_field= NULL;
  timestamp_field= NULL;

  pos_in_table_list= NULL;
  group= NULL;
  alias= NULL;
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

  memory::init_sql_alloc(&mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  memset(&sort, 0, sizeof(filesort_info_st));
}



/* Deallocate temporary blob storage */

void free_blobs(register Table *table)
{
  uint32_t *ptr, *end;
  for (ptr= table->getBlobField(), end=ptr + table->sizeBlobFields();
       ptr != end ;
       ptr++)
    ((Field_blob*) table->field[*ptr])->free();
}


	/* error message when opening a form cursor */

void TableShare::open_table_error(int pass_error, int db_errno, int pass_errarg)
{
  int err_no;
  char buff[FN_REFLEN];
  myf errortype= ME_ERROR+ME_WAITTANG;

  switch (pass_error) {
  case 7:
  case 1:
    if (db_errno == ENOENT)
      my_error(ER_NO_SUCH_TABLE, MYF(0), db.str, table_name.str);
    else
    {
      sprintf(buff,"%s",normalized_path.str);
      my_error((db_errno == EMFILE) ? ER_CANT_OPEN_FILE : ER_FILE_NOT_FOUND,
               errortype, buff, db_errno);
    }
    break;
  case 2:
  {
    Cursor *cursor= 0;
    const char *datext= "";

    if (db_type() != NULL)
    {
      if ((cursor= db_type()->getCursor(*this, current_session->mem_root)))
      {
        if (!(datext= *db_type()->bas_ext()))
          datext= "";
      }
    }
    err_no= (db_errno == ENOENT) ? ER_FILE_NOT_FOUND : (db_errno == EAGAIN) ?
      ER_FILE_USED : ER_CANT_OPEN_FILE;
    sprintf(buff,"%s%s", normalized_path.str,datext);
    my_error(err_no,errortype, buff, db_errno);
    delete cursor;
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
    sprintf(buff,"%s", normalized_path.str);
    my_printf_error(ER_NOT_FORM_FILE,
                    _("Table '%-.64s' was created with a different version "
                    "of Drizzle and cannot be read"),
                    MYF(0), buff);
    break;
  case 8:
    break;
  default:				/* Better wrong error than none */
  case 4:
    sprintf(buff,"%s", normalized_path.str);
    my_error(ER_NOT_FORM_FILE, errortype, buff, 0);
    break;
  }
  return;
} /* open_table_error */


TYPELIB *typelib(memory::Root *mem_root, List<String> &strings)
{
  TYPELIB *result= (TYPELIB*) alloc_root(mem_root, sizeof(TYPELIB));
  if (!result)
    return 0;
  result->count= strings.elements;
  result->name= "";
  uint32_t nbytes= (sizeof(char*) + sizeof(uint32_t)) * (result->count + 1);
  
  if (!(result->type_names= (const char**) alloc_root(mem_root, nbytes)))
    return 0;
    
  result->type_lengths= (uint*) (result->type_names + result->count + 1);

  List_iterator<String> it(strings);
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

int set_zone(register int nr, int min_zone, int max_zone)
{
  if (nr<=min_zone)
    return (min_zone);
  if (nr>=max_zone)
    return (max_zone);
  return (nr);
} /* set_zone */

	/* Adjust number to next larger disk buffer */

ulong next_io_size(register ulong pos)
{
  register ulong offset;
  if ((offset= pos & (IO_SIZE-1)))
    return pos-offset+IO_SIZE;
  return pos;
} /* next_io_size */


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
      pos+= mblen;
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
  Set up column usage bitmaps for a temporary table

  IMPLEMENTATION
    For temporary tables, we need one bitmap with all columns set and
    a tmp_set bitmap to be used by things like filesort.
*/

void Table::setup_tmp_table_column_bitmaps(unsigned char *bitmaps)
{
  uint32_t field_count= s->fields;

  this->def_read_set.init((my_bitmap_map*) bitmaps, field_count);
  this->tmp_set.init((my_bitmap_map*) (bitmaps+ bitmap_buffer_size(field_count)), field_count);

  /* write_set and all_set are copies of read_set */
  def_write_set= def_read_set;
  s->all_set= def_read_set;
  this->s->all_set.setAll();
  default_column_bitmaps();
}



void Table::updateCreateInfo(message::Table *table_proto)
{
  message::Table::TableOptions *table_options= table_proto->mutable_options();
  table_options->set_block_size(s->block_size);
  table_options->set_comment(s->getComment());
}

int rename_file_ext(const char * from,const char * to,const char * ext)
{
  string from_s, to_s;

  from_s.append(from);
  from_s.append(ext);
  to_s.append(to);
  to_s.append(ext);
  return (internal::my_rename(from_s.c_str(),to_s.c_str(),MYF(MY_WME)));
}

/*
  DESCRIPTION
    given a buffer with a key value, and a map of keyparts
    that are present in this value, returns the length of the value
*/
uint32_t calculate_key_len(Table *table, uint32_t key,
                       const unsigned char *,
                       key_part_map keypart_map)
{
  /* works only with key prefixes */
  assert(((keypart_map + 1) & keypart_map) == 0);

  KEY *key_info= table->s->key_info+key;
  KEY_PART_INFO *key_part= key_info->key_part;
  KEY_PART_INFO *end_key_part= key_part + key_info->key_parts;
  uint32_t length= 0;

  while (key_part < end_key_part && keypart_map)
  {
    length+= key_part->store_length;
    keypart_map >>= 1;
    key_part++;
  }
  return length;
}

/*
  Check if database name is valid

  SYNPOSIS
    check_db_name()
    org_name		Name of database and length

  RETURN
    0	ok
    1   error
*/

bool check_db_name(LEX_STRING *org_name)
{
  char *name= org_name->str;
  uint32_t name_length= org_name->length;

  if (not plugin::Authorization::isAuthorized(current_session->getSecurityContext(),
                                              string(name, name_length)))
  {
    return 1;
  }

  if (!name_length || name_length > NAME_LEN || name[name_length - 1] == ' ')
    return 1;

  my_casedn_str(files_charset_info, name);

  return check_identifier_name(org_name);
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
  def_read_set.clearAll();
  def_write_set.clearAll();
  column_bitmaps_set(&def_read_set, &def_write_set);
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
      s->primary_key < MAX_KEY)
  {
    mark_columns_used_by_index_no_reset(s->primary_key);
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
  MyBitmap *bitmap= &tmp_set;

  (void) cursor->extra(HA_EXTRA_KEYREAD);
  bitmap->clearAll();
  mark_columns_used_by_index_no_reset(index, bitmap);
  column_bitmaps_set(bitmap, bitmap);
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
    mark_columns_used_by_index_no_reset(index, read_set);
}

void Table::mark_columns_used_by_index_no_reset(uint32_t index,
                                                MyBitmap *bitmap)
{
  KEY_PART_INFO *key_part= key_info[index].key_part;
  KEY_PART_INFO *key_part_end= (key_part +
                                key_info[index].key_parts);
  for (;key_part != key_part_end; key_part++)
    bitmap->setBit(key_part->fieldnr-1);
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
  setReadSet(found_next_number_field->field_index);
  setWriteSet(found_next_number_field->field_index);
  if (s->next_number_keypart)
    mark_columns_used_by_index_no_reset(s->next_number_index);
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
  if (s->primary_key == MAX_KEY)
  {
    /* fallback to use all columns in the table to identify row */
    use_all_columns();
    return;
  }
  else
    mark_columns_used_by_index_no_reset(s->primary_key);

  /* If we the engine wants all predicates we mark all keys */
  if (cursor->getEngine()->check_flag(HTON_BIT_REQUIRES_KEY_COLUMNS_FOR_DELETE))
  {
    Field **reg_field;
    for (reg_field= field ; *reg_field ; reg_field++)
    {
      if ((*reg_field)->flags & PART_KEY_FLAG)
        setReadSet((*reg_field)->field_index);
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
  if (s->primary_key == MAX_KEY)
  {
    /* fallback to use all columns in the table to identify row */
    use_all_columns();
    return;
  }
  else
    mark_columns_used_by_index_no_reset(s->primary_key);

  if (cursor->getEngine()->check_flag(HTON_BIT_REQUIRES_KEY_COLUMNS_FOR_DELETE))
  {
    /* Mark all used key columns for read */
    Field **reg_field;
    for (reg_field= field ; *reg_field ; reg_field++)
    {
      /* Merge keys is all keys that had a column refered to in the query */
      if (is_overlapping(merge_keys, (*reg_field)->part_of_key))
        setReadSet((*reg_field)->field_index);
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
                              (data + blob->offset(record[0]))) +
      HA_KEY_BLOB_LENGTH;
  }
  return length;
}

/****************************************************************************
 Functions for creating temporary tables.
****************************************************************************/


/* Prototypes */
void free_tmp_table(Session *session, Table *entry);

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
  if (convert_blob_length && convert_blob_length <= Field_varstring::MAX_SIZE &&
      (org_field->flags & BLOB_FLAG))
    new_field= new Field_varstring(convert_blob_length,
                                   org_field->maybe_null(),
                                   org_field->field_name, table->s,
                                   org_field->charset());
  else
    new_field= org_field->new_field(session->mem_root, table,
                                    table == org_field->table);
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
      table->s->db_create_options|= HA_OPTION_PACK_RECORD;
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

#define STRING_TOTAL_LENGTH_TO_PACK_ROWS 128
#define AVG_STRING_LENGTH_TO_PACK_ROWS   64
#define RATIO_TO_PACK_ROWS	       2

static void make_internal_temporary_table_path(Session *session, char *path)
{
  snprintf(path, FN_REFLEN, "%s%lx_%"PRIx64"_%x", TMP_FILE_PREFIX, (unsigned long)current_pid,
           session->thread_id, session->tmp_table++);

  internal::fn_format(path, path, drizzle_tmpdir, "", MY_REPLACE_EXT|MY_UNPACK_FILENAME);
}

Table *
create_tmp_table(Session *session,Tmp_Table_Param *param,List<Item> &fields,
		 order_st *group, bool distinct, bool save_sum_fields,
		 uint64_t select_options, ha_rows rows_limit,
		 const char *table_alias)
{
  memory::Root *mem_root_save, own_root;
  Table *table;
  TableShare *share;
  uint	i,field_count,null_count,null_pack_length;
  uint32_t  copy_func_count= param->func_count;
  uint32_t  hidden_null_count, hidden_null_pack_length, hidden_field_count;
  uint32_t  blob_count,group_null_items, string_count;
  uint32_t fieldnr= 0;
  ulong reclength, string_total_length;
  bool  using_unique_constraint= false;
  bool  use_packed_rows= true;
  bool  not_all_columns= !(select_options & TMP_TABLE_ALL_COLUMNS);
  char  *tmpname;
  char  path[FN_REFLEN];
  unsigned char	*pos, *group_buff, *bitmaps;
  unsigned char *null_flags;
  Field **reg_field, **from_field, **default_field;
  uint32_t *blob_field;
  CopyField *copy= 0;
  KEY *keyinfo;
  KEY_PART_INFO *key_part_info;
  Item **copy_func;
  MI_COLUMNDEF *recinfo;
  uint32_t total_uneven_bit_length= 0;
  bool force_copy_fields= param->force_copy_fields;
  uint64_t max_rows= 0;

  status_var_increment(session->status_var.created_tmp_tables);

  make_internal_temporary_table_path(session, path);

  if (group)
  {
    if (! param->quick_group)
      group= 0;					// Can't use group key
    else for (order_st *tmp=group ; tmp ; tmp=tmp->next)
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
    copy_func_count+= param->sum_func_count;

  memory::init_sql_alloc(&own_root, TABLE_ALLOC_BLOCK_SIZE, 0);

  if (!multi_alloc_root(&own_root,
                        &table, sizeof(*table),
                        &share, sizeof(*share),
                        &reg_field, sizeof(Field*) * (field_count+1),
                        &default_field, sizeof(Field*) * (field_count),
                        &blob_field, sizeof(uint32_t)*(field_count+1),
                        &from_field, sizeof(Field*)*field_count,
                        &copy_func, sizeof(*copy_func)*(copy_func_count+1),
                        &param->keyinfo, sizeof(*param->keyinfo),
                        &key_part_info,
                        sizeof(*key_part_info)*(param->group_parts+1),
                        &param->start_recinfo,
                        sizeof(*param->recinfo)*(field_count*2+4),
                        &tmpname, (uint32_t) strlen(path)+1,
                        &group_buff, (group && ! using_unique_constraint ?
                                      param->group_length : 0),
                        &bitmaps, bitmap_buffer_size(field_count)*2,
                        NULL))
  {
    return NULL;
  }
  /* CopyField belongs to Tmp_Table_Param, allocate it in Session mem_root */
  if (!(param->copy_field= copy= new (session->mem_root) CopyField[field_count]))
  {
    free_root(&own_root, MYF(0));
    return NULL;
  }
  param->items_to_copy= copy_func;
  strcpy(tmpname,path);
  /* make table according to fields */

  memset(table, 0, sizeof(*table));
  memset(reg_field, 0, sizeof(Field*)*(field_count+1));
  memset(default_field, 0, sizeof(Field*) * (field_count));
  memset(from_field, 0, sizeof(Field*)*field_count);

  table->mem_root= own_root;
  mem_root_save= session->mem_root;
  session->mem_root= &table->mem_root;

  table->field=reg_field;
  table->alias= table_alias;
  table->reginfo.lock_type=TL_WRITE;	/* Will be updated */
  table->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  table->map=1;
  table->copy_blobs= 1;
  table->in_use= session;
  table->quick_keys.reset();
  table->covering_keys.reset();
  table->keys_in_use_for_query.reset();

  table->setShare(share);
  share->init(tmpname, tmpname);
  share->blob_field= blob_field;
  share->blob_ptr_size= portable_sizeof_char_ptr;
  share->db_low_byte_first=1;                // True for HEAP and MyISAM
  share->table_charset= param->table_charset;
  share->primary_key= MAX_KEY;               // Indicate no primary key
  share->keys_for_keyread.reset();
  share->keys_in_use.reset();

  /* Calculate which type of fields we will store in the temporary table */

  reclength= string_total_length= 0;
  blob_count= string_count= null_count= hidden_null_count= group_null_items= 0;
  param->using_indirect_summary_function= 0;

  List_iterator_fast<Item> li(fields);
  Item *item;
  Field **tmp_from_field=from_field;
  while ((item=li++))
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
          session->change_item_tree(argp, new Item_field(new_field));
          session->mem_root= &table->mem_root;
	  if (!(new_field->flags & NOT_NULL_FLAG))
          {
	    null_count++;
            /*
              new_field->maybe_null() is still false, it will be
              changed below. But we have to setup Item_field correctly
            */
            (*argp)->maybe_null=1;
          }
          new_field->field_index= fieldnr++;
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
      new_field->field_index= fieldnr++;
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
  assert(fieldnr == (uint32_t) (reg_field - table->field));
  assert(field_count >= (uint32_t) (reg_field - table->field));
  field_count= fieldnr;
  *reg_field= 0;
  *blob_field= 0;				// End marker
  share->fields= field_count;

  /* If result table is small; use a heap */
  /* future: storage engine selection can be made dynamic? */
  if (blob_count || using_unique_constraint ||
      (select_options & (OPTION_BIG_TABLES | SELECT_SMALL_RESULT)) == OPTION_BIG_TABLES)
  {
    share->storage_engine= myisam_engine;
    table->cursor= share->db_type()->getCursor(*share, &table->mem_root);
    if (group &&
	(param->group_parts > table->cursor->getEngine()->max_key_parts() ||
	 param->group_length > table->cursor->getEngine()->max_key_length()))
      using_unique_constraint= true;
  }
  else
  {
    share->storage_engine= heap_engine;
    table->cursor= share->db_type()->getCursor(*share, &table->mem_root);
  }
  if (! table->cursor)
    goto err;


  if (! using_unique_constraint)
    reclength+= group_null_items;	// null flag is stored separately

  share->blob_fields= blob_count;
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
  /* Use packed rows if there is blobs or a lot of space to gain */
  if (blob_count || ((string_total_length >= STRING_TOTAL_LENGTH_TO_PACK_ROWS) && (reclength / string_total_length <= RATIO_TO_PACK_ROWS || (string_total_length / string_count) >= AVG_STRING_LENGTH_TO_PACK_ROWS)))
    use_packed_rows= 1;

  share->reclength= reclength;
  {
    uint32_t alloc_length=ALIGN_SIZE(reclength+MI_UNIQUE_HASH_LENGTH+1);
    share->rec_buff_length= alloc_length;
    if (!(table->record[0]= (unsigned char*)
                            alloc_root(&table->mem_root, alloc_length*3)))
      goto err;
    table->record[1]= table->record[0]+alloc_length;
    share->default_values= table->record[1]+alloc_length;
  }
  copy_func[0]= 0;				// End marker
  param->func_count= copy_func - param->items_to_copy;

  table->setup_tmp_table_column_bitmaps(bitmaps);

  recinfo=param->start_recinfo;
  null_flags=(unsigned char*) table->record[0];
  pos=table->record[0]+ null_pack_length;
  if (null_pack_length)
  {
    memset(recinfo, 0, sizeof(*recinfo));
    recinfo->type=FIELD_NORMAL;
    recinfo->length=null_pack_length;
    recinfo++;
    memset(null_flags, 255, null_pack_length);	// Set null fields

    table->null_flags= (unsigned char*) table->record[0];
    share->null_fields= null_count+ hidden_null_count;
    share->null_bytes= null_pack_length;
  }
  null_count= (blob_count == 0) ? 1 : 0;
  hidden_field_count=param->hidden_field_count;
  for (i= 0,reg_field=table->field; i < field_count; i++,reg_field++,recinfo++)
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
      diff= (ptrdiff_t) (orig_field->table->s->default_values-
                            orig_field->table->record[0]);
      orig_field->move_field_offset(diff);      // Points now at default_values
      if (orig_field->is_real_null())
        field->set_null();
      else
      {
        field->set_notnull();
        memcpy(field->ptr, orig_field->ptr, field->pack_length());
      }
      orig_field->move_field_offset(-diff);     // Back to record[0]
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

    // fix table name in field entry
    field->table_name= &table->alias;
  }

  param->copy_field_end=copy;
  param->recinfo=recinfo;
  table->storeRecordAsDefault();        // Make empty default record

  if (session->variables.tmp_table_size == ~ (uint64_t) 0)		// No limit
    max_rows= ~(uint64_t) 0;
  else
    max_rows= (uint64_t) (((share->db_type() == heap_engine) ?
                          min(session->variables.tmp_table_size,
                              session->variables.max_heap_table_size) :
                          session->variables.tmp_table_size) /
                         share->reclength);

  set_if_bigger(max_rows, (uint64_t)1);	// For dummy start options
  /*
    Push the LIMIT clause to the temporary table creation, so that we
    materialize only up to 'rows_limit' records instead of all result records.
  */
  set_if_smaller(max_rows, rows_limit);

  share->setMaxRows(max_rows);

  param->end_write_records= rows_limit;

  keyinfo= param->keyinfo;

  if (group)
  {
    table->group=group;				/* Table is grouped by key */
    param->group_buff=group_buff;
    share->keys=1;
    share->uniques= test(using_unique_constraint);
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME;
    keyinfo->usable_key_parts=keyinfo->key_parts= param->group_parts;
    keyinfo->key_length= 0;
    keyinfo->rec_per_key= 0;
    keyinfo->algorithm= HA_KEY_ALG_UNDEF;
    keyinfo->name= (char*) "group_key";
    order_st *cur_group= group;
    for (; cur_group ; cur_group= cur_group->next, key_part_info++)
    {
      Field *field=(*cur_group->item)->get_tmp_table_field();
      bool maybe_null=(*cur_group->item)->maybe_null;
      key_part_info->null_bit= 0;
      key_part_info->field=  field;
      key_part_info->offset= field->offset(table->record[0]);
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
					      (unsigned char*) table->record[0]);
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
      share->uniques= 1;
    }
    null_pack_length-=hidden_null_pack_length;
    keyinfo->key_parts= ((field_count-param->hidden_field_count)+
			 (share->uniques ? test(null_pack_length) : 0));
    table->distinct= 1;
    share->keys= 1;
    if (!(key_part_info= (KEY_PART_INFO*)
          alloc_root(&table->mem_root,
                     keyinfo->key_parts * sizeof(KEY_PART_INFO))))
      goto err;
    memset(key_part_info, 0, keyinfo->key_parts * sizeof(KEY_PART_INFO));
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
    if (null_pack_length && share->uniques)
    {
      key_part_info->null_bit= 0;
      key_part_info->offset=hidden_null_pack_length;
      key_part_info->length=null_pack_length;
      key_part_info->field= new Field_varstring(table->record[0],
                                                (uint32_t) key_part_info->length,
                                                0,
                                                (unsigned char*) 0,
                                                (uint32_t) 0,
                                                NULL,
                                                table->s,
                                                &my_charset_bin);
      if (!key_part_info->field)
        goto err;
      key_part_info->field->init(table);
      key_part_info->key_type= 1; /* binary comparison */
      key_part_info->type=    HA_KEYTYPE_BINARY;
      key_part_info++;
    }
    /* Create a distinct key over the columns we are going to return */
    for (i=param->hidden_field_count, reg_field=table->field + i ;
	 i < field_count;
	 i++, reg_field++, key_part_info++)
    {
      key_part_info->null_bit= 0;
      key_part_info->field=    *reg_field;
      key_part_info->offset=   (*reg_field)->offset(table->record[0]);
      key_part_info->length=   (uint16_t) (*reg_field)->pack_length();
      /* TODO:
        The below method of computing the key format length of the
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
  share->db_record_offset= 1;
  if (share->db_type() == myisam_engine)
  {
    if (table->create_myisam_tmp_table(param->keyinfo, param->start_recinfo,
				       &param->recinfo, select_options))
      goto err;
  }
  if (table->open_tmp_table())
    goto err;

  session->mem_root= mem_root_save;

  return(table);

err:
  session->mem_root= mem_root_save;
  table->free_tmp_table(session);
  return NULL;
}

/****************************************************************************/

/**
  Create a reduced Table object with properly set up Field list from a
  list of field definitions.

    The created table doesn't have a table Cursor associated with
    it, has no keys, no group/distinct, no copy_funcs array.
    The sole purpose of this Table object is to use the power of Field
    class to read/write data to/from table->record[0]. Then one can store
    the record in any container (RB tree, hash, etc).
    The table is created in Session mem_root, so are the table's fields.
    Consequently, if you don't BLOB fields, you don't need to free it.

  @param session         connection handle
  @param field_list  list of column definitions

  @return
    0 if out of memory, Table object in case of success
*/

Table *create_virtual_tmp_table(Session *session, List<CreateField> &field_list)
{
  uint32_t field_count= field_list.elements;
  uint32_t blob_count= 0;
  Field **field;
  CreateField *cdef;                           /* column definition */
  uint32_t record_length= 0;
  uint32_t null_count= 0;                 /* number of columns which may be null */
  uint32_t null_pack_length;              /* NULL representation array length */
  uint32_t *blob_field;
  unsigned char *bitmaps;
  Table *table;
  TableShare *share;

  if (!multi_alloc_root(session->mem_root,
                        &table, sizeof(*table),
                        &share, sizeof(*share),
                        &field, (field_count + 1) * sizeof(Field*),
                        &blob_field, (field_count+1) *sizeof(uint32_t),
                        &bitmaps, bitmap_buffer_size(field_count)*2,
                        NULL))
    return NULL;

  memset(table, 0, sizeof(*table));
  memset(share, 0, sizeof(*share));
  table->field= field;
  table->s= share;
  share->blob_field= blob_field;
  share->fields= field_count;
  share->blob_ptr_size= portable_sizeof_char_ptr;
  table->setup_tmp_table_column_bitmaps(bitmaps);

  /* Create all fields and calculate the total length of record */
  List_iterator_fast<CreateField> it(field_list);
  while ((cdef= it++))
  {
    *field= make_field(share,
                       NULL,
                       0,
                       cdef->length,
                       (cdef->flags & NOT_NULL_FLAG) ? false : true,
                       (unsigned char *) ((cdef->flags & NOT_NULL_FLAG) ? 0 : ""),
                       (cdef->flags & NOT_NULL_FLAG) ? 0 : 1,
                       cdef->decimals,
                       cdef->sql_type,
                       cdef->charset,
                       cdef->unireg_check,
                       cdef->interval,
                       cdef->field_name);
    if (!*field)
      goto error;
    (*field)->init(table);
    record_length+= (*field)->pack_length();
    if (! ((*field)->flags & NOT_NULL_FLAG))
      null_count++;

    if ((*field)->flags & BLOB_FLAG)
      share->blob_field[blob_count++]= (uint32_t) (field - table->field);

    field++;
  }
  *field= NULL;                             /* mark the end of the list */
  share->blob_field[blob_count]= 0;            /* mark the end of the list */
  share->blob_fields= blob_count;

  null_pack_length= (null_count + 7)/8;
  share->reclength= record_length + null_pack_length;
  share->rec_buff_length= ALIGN_SIZE(share->reclength + 1);
  table->record[0]= (unsigned char*) session->alloc(share->rec_buff_length);
  if (!table->record[0])
    goto error;

  if (null_pack_length)
  {
    table->null_flags= (unsigned char*) table->record[0];
    share->null_fields= null_count;
    share->null_bytes= null_pack_length;
  }

  table->in_use= session;           /* field->reset() may access table->in_use */
  {
    /* Set up field pointers */
    unsigned char *null_pos= table->record[0];
    unsigned char *field_pos= null_pos + share->null_bytes;
    uint32_t null_bit= 1;

    for (field= table->field; *field; ++field)
    {
      Field *cur_field= *field;
      if ((cur_field->flags & NOT_NULL_FLAG))
        cur_field->move_field(field_pos);
      else
      {
        cur_field->move_field(field_pos, (unsigned char*) null_pos, null_bit);
        null_bit<<= 1;
        if (null_bit == (1 << 8))
        {
          ++null_pos;
          null_bit= 1;
        }
      }
      cur_field->reset();

      field_pos+= cur_field->pack_length();
    }
  }
  return table;
error:
  for (field= table->field; *field; ++field)
    delete *field;                         /* just invokes field destructor */
  return 0;
}

bool Table::open_tmp_table()
{
  int error;
  if ((error=cursor->ha_open(this, s->table_name.str,O_RDWR,
                                  HA_OPEN_TMP_TABLE | HA_OPEN_INTERNAL_TABLE)))
  {
    print_error(error, MYF(0));
    db_stat= 0;
    return true;
  }
  (void) cursor->extra(HA_EXTRA_QUICK);		/* Faster */
  return false;
}


/*
  Create MyISAM temporary table

  SYNOPSIS
    create_myisam_tmp_table()
      keyinfo         Description of the index (there is always one index)
      start_recinfo   MyISAM's column descriptions
      recinfo INOUT   End of MyISAM's column descriptions
      options         Option bits

  DESCRIPTION
    Create a MyISAM temporary table according to passed description. The is
    assumed to have one unique index or constraint.

    The passed array or MI_COLUMNDEF structures must have this form:

      1. 1-byte column (afaiu for 'deleted' flag) (note maybe not 1-byte
         when there are many nullable columns)
      2. Table columns
      3. One free MI_COLUMNDEF element (*recinfo points here)

    This function may use the free element to create hash column for unique
    constraint.

   RETURN
     false - OK
     true  - Error
*/

bool Table::create_myisam_tmp_table(KEY *keyinfo,
                                    MI_COLUMNDEF *start_recinfo,
                                    MI_COLUMNDEF **recinfo,
				    uint64_t options)
{
  int error;
  MI_KEYDEF keydef;
  MI_UNIQUEDEF uniquedef;
  TableShare *share= s;

  if (share->keys)
  {						// Get keys for ni_create
    bool using_unique_constraint= false;
    HA_KEYSEG *seg= (HA_KEYSEG*) alloc_root(&this->mem_root,
                                            sizeof(*seg) * keyinfo->key_parts);
    if (!seg)
      goto err;

    memset(seg, 0, sizeof(*seg) * keyinfo->key_parts);
    if (keyinfo->key_length >= cursor->getEngine()->max_key_length() ||
	keyinfo->key_parts > cursor->getEngine()->max_key_parts() ||
	share->uniques)
    {
      /* Can't create a key; Make a unique constraint instead of a key */
      share->keys=    0;
      share->uniques= 1;
      using_unique_constraint= true;
      memset(&uniquedef, 0, sizeof(uniquedef));
      uniquedef.keysegs=keyinfo->key_parts;
      uniquedef.seg=seg;
      uniquedef.null_are_equal=1;

      /* Create extra column for hash value */
      memset(*recinfo, 0, sizeof(**recinfo));
      (*recinfo)->type= FIELD_CHECK;
      (*recinfo)->length=MI_UNIQUE_HASH_LENGTH;
      (*recinfo)++;
      share->reclength+=MI_UNIQUE_HASH_LENGTH;
    }
    else
    {
      /* Create an unique key */
      memset(&keydef, 0, sizeof(keydef));
      keydef.flag=HA_NOSAME | HA_BINARY_PACK_KEY | HA_PACK_KEY;
      keydef.keysegs=  keyinfo->key_parts;
      keydef.seg= seg;
    }
    for (uint32_t i= 0; i < keyinfo->key_parts ; i++,seg++)
    {
      Field *key_field=keyinfo->key_part[i].field;
      seg->flag=     0;
      seg->language= key_field->charset()->number;
      seg->length=   keyinfo->key_part[i].length;
      seg->start=    keyinfo->key_part[i].offset;
      if (key_field->flags & BLOB_FLAG)
      {
	seg->type= ((keyinfo->key_part[i].key_type & 1 /* binary */) ?
	 HA_KEYTYPE_VARBINARY2 : HA_KEYTYPE_VARTEXT2);
	seg->bit_start= (uint8_t)(key_field->pack_length()
                                  - share->blob_ptr_size);
	seg->flag= HA_BLOB_PART;
	seg->length= 0;			// Whole blob in unique constraint
      }
      else
      {
	seg->type= keyinfo->key_part[i].type;
      }
      if (!(key_field->flags & NOT_NULL_FLAG))
      {
	seg->null_bit= key_field->null_bit;
	seg->null_pos= (uint32_t) (key_field->null_ptr - (unsigned char*) record[0]);
	/*
	  We are using a GROUP BY on something that contains NULL
	  In this case we have to tell MyISAM that two NULL should
	  on INSERT be regarded at the same value
	*/
	if (! using_unique_constraint)
	  keydef.flag|= HA_NULL_ARE_EQUAL;
      }
    }
  }
  MI_CREATE_INFO create_info;
  memset(&create_info, 0, sizeof(create_info));

  if ((options & (OPTION_BIG_TABLES | SELECT_SMALL_RESULT)) ==
      OPTION_BIG_TABLES)
    create_info.data_file_length= ~(uint64_t) 0;

  if ((error=mi_create(share->table_name.str, share->keys, &keydef,
		       (uint32_t) (*recinfo-start_recinfo),
		       start_recinfo,
		       share->uniques, &uniquedef,
		       &create_info,
		       HA_CREATE_TMP_TABLE)))
  {
    print_error(error, MYF(0));
    db_stat= 0;
    goto err;
  }
  status_var_increment(in_use->status_var.created_tmp_disk_tables);
  share->db_record_offset= 1;
  return false;
 err:
  return true;
}


void Table::free_tmp_table(Session *session)
{
  memory::Root own_root= mem_root;
  const char *save_proc_info;

  save_proc_info=session->get_proc_info();
  session->set_proc_info("removing tmp table");

  // Release latches since this can take a long time
  plugin::TransactionalStorageEngine::releaseTemporaryLatches(session);

  if (cursor)
  {
    if (db_stat)
      cursor->closeMarkForDelete(s->table_name.str);

    TableIdentifier identifier(s->getSchemaName(), s->table_name.str, s->table_name.str);
    s->db_type()->doDropTable(*session, identifier);

    delete cursor;
  }

  /* free blobs */
  for (Field **ptr= field ; *ptr ; ptr++)
    (*ptr)->free();
  free_io_cache();

  free_root(&own_root, MYF(0)); /* the table is allocated in its own root */
  session->set_proc_info(save_proc_info);
}

/**
  If a HEAP table gets full, create a MyISAM table and copy all rows
  to this.
*/

bool create_myisam_from_heap(Session *session, Table *table,
                             MI_COLUMNDEF *start_recinfo,
                             MI_COLUMNDEF **recinfo,
			     int error, bool ignore_last_dupp_key_error)
{
  Table new_table;
  TableShare share;
  const char *save_proc_info;
  int write_err;

  if (table->s->db_type() != heap_engine ||
      error != HA_ERR_RECORD_FILE_FULL)
  {
    table->print_error(error, MYF(0));
    return true;
  }

  // Release latches since this can take a long time
  plugin::TransactionalStorageEngine::releaseTemporaryLatches(session);

  new_table= *table;
  share= *table->s;
  new_table.s= &share;
  new_table.s->storage_engine= myisam_engine;
  if (not (new_table.cursor= new_table.s->db_type()->getCursor(share, &new_table.mem_root)))
    return true;				// End of memory

  save_proc_info=session->get_proc_info();
  session->set_proc_info("converting HEAP to MyISAM");

  if (new_table.create_myisam_tmp_table(table->key_info, start_recinfo,
					recinfo, session->lex->select_lex.options |
					session->options))
    goto err2;
  if (new_table.open_tmp_table())
    goto err1;
  if (table->cursor->indexes_are_disabled())
    new_table.cursor->ha_disable_indexes(HA_KEY_SWITCH_ALL);
  table->cursor->ha_index_or_rnd_end();
  table->cursor->ha_rnd_init(1);
  if (table->no_rows)
  {
    new_table.cursor->extra(HA_EXTRA_NO_ROWS);
    new_table.no_rows=1;
  }

  /* HA_EXTRA_WRITE_CACHE can stay until close, no need to disable it */
  new_table.cursor->extra(HA_EXTRA_WRITE_CACHE);

  /*
    copy all old rows from heap table to MyISAM table
    This is the only code that uses record[1] to read/write but this
    is safe as this is a temporary MyISAM table without timestamp/autoincrement.
  */
  while (!table->cursor->rnd_next(new_table.record[1]))
  {
    write_err= new_table.cursor->ha_write_row(new_table.record[1]);
    if (write_err)
      goto err;
  }
  /* copy row that filled HEAP table */
  if ((write_err=new_table.cursor->ha_write_row(table->record[0])))
  {
    if (new_table.cursor->is_fatal_error(write_err, HA_CHECK_DUP) ||
	!ignore_last_dupp_key_error)
      goto err;
  }

  /* remove heap table and change to use myisam table */
  (void) table->cursor->ha_rnd_end();
  (void) table->cursor->close();                  // This deletes the table !
  delete table->cursor;
  table->cursor= NULL;
  new_table.s= table->s;                       // Keep old share
  *table= new_table;
  *table->s= share;

  table->cursor->change_table_ptr(table, table->s);
  table->use_all_columns();
  if (save_proc_info)
  {
    const char *new_proc_info=
      (!strcmp(save_proc_info,"Copying to tmp table") ?
      "Copying to tmp table on disk" : save_proc_info);
    session->set_proc_info(new_proc_info);
  }
  return false;

 err:
  table->print_error(write_err, MYF(0));
  (void) table->cursor->ha_rnd_end();
  (void) new_table.cursor->close();

 err1:
  {
    TableIdentifier identifier(new_table.s->getSchemaName(), new_table.s->table_name.str, new_table.s->table_name.str);
    new_table.s->db_type()->doDropTable(*session, identifier);
  }

 err2:
  delete new_table.cursor;
  session->set_proc_info(save_proc_info);
  table->mem_root= new_table.mem_root;
  return true;
}

my_bitmap_map *Table::use_all_columns(MyBitmap *bitmap)
{
  my_bitmap_map *old= bitmap->getBitmap();
  bitmap->setBitmap(s->all_set.getBitmap());
  return old;
}

void Table::restore_column_map(my_bitmap_map *old)
{
  read_set->setBitmap(old);
}

uint32_t Table::find_shortest_key(const key_map *usable_keys)
{
  uint32_t min_length= UINT32_MAX;
  uint32_t best= MAX_KEY;
  if (usable_keys->any())
  {
    for (uint32_t nr= 0; nr < s->keys ; nr++)
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
    if ((*ptr)->cmp_offset(s->rec_buff_length))
      return true;
  }
  return false;
}

/* Return false if row hasn't changed */

bool Table::compare_record()
{
  if (s->blob_fields + s->varchar_fields == 0)
    return memcmp(this->record[0], this->record[1], (size_t) s->reclength);
  
  /* Compare null bits */
  if (memcmp(null_flags, null_flags + s->rec_buff_length, s->null_bytes))
    return true; /* Diff in NULL value */

  /* Compare updated fields */
  for (Field **ptr= field ; *ptr ; ptr++)
  {
    if (isWriteSet((*ptr)->field_index) &&
	(*ptr)->cmp_binary_offset(s->rec_buff_length))
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
  memcpy(record[1], record[0], (size_t) s->reclength);
}

/*
 * Store a record as an insert
 *
 */
void Table::storeRecordAsInsert()
{
  memcpy(insert_values, record[0], (size_t) s->reclength);
}

/*
 * Store a record with default values
 *
 */
void Table::storeRecordAsDefault()
{
  memcpy(s->default_values, record[0], (size_t) s->reclength);
}

/*
 * Restore a record from previous record into next
 *
 */
void Table::restoreRecord()
{
  memcpy(record[0], record[1], (size_t) s->reclength);
}

/*
 * Restore a record with default values
 *
 */
void Table::restoreRecordAsDefault()
{
  memcpy(record[0], s->default_values, (size_t) s->reclength);
}

/*
 * Empty a record
 *
 */
void Table::emptyRecord()
{
  restoreRecordAsDefault();
  memset(null_flags, 255, s->null_bytes);
}

Table::Table()
  : s(NULL),
    field(NULL),
    cursor(NULL),
    next(NULL),
    prev(NULL),
    read_set(NULL),
    write_set(NULL),
    tablenr(0),
    db_stat(0),
    in_use(NULL),
    insert_values(NULL),
    key_info(NULL),
    next_number_field(NULL),
    found_next_number_field(NULL),
    timestamp_field(NULL),
    pos_in_table_list(NULL),
    group(NULL),
    alias(NULL),
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
    map(0)
{
  record[0]= (unsigned char *) 0;
  record[1]= (unsigned char *) 0;

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

  memory::init_sql_alloc(&mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  memset(&sort, 0, sizeof(filesort_info_st));
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
    errmsg_printf(ERRMSG_LVL_ERROR, _("Got error %d when reading table '%s'"),
		    error, s->path.str);
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
  TableList *embedding= table_list->embedding;
  while (!maybe_null && embedding)
  {
    maybe_null= embedding->outer_join;
    embedding= embedding->embedding;
  }
  tablenr= table_number;
  map= (table_map) 1 << table_number;
  force_index= table_list->force_index;
  covering_keys= s->keys_for_keyread;
  merge_keys.reset();
}


/*
  Used by ALTER Table when the table is a temporary one. It changes something
  only if the ALTER contained a RENAME clause (otherwise, table_name is the old
  name).
  Prepares a table cache key, which is the concatenation of db, table_name and
  session->slave_proxy_id, separated by '\0'.
*/

bool Table::renameAlterTemporaryTable(TableIdentifier &identifier)
{
  char *key;
  uint32_t key_length;
  TableShare *share= s;

  if (not (key=(char*) alloc_root(&share->mem_root, MAX_DBKEY_LENGTH)))
    return true;

  key_length= TableShare::createKey(key, identifier);
  share->set_table_cache_key(key, key_length);

  message::Table *message= share->getTableProto();

  message->set_name(identifier.getTableName());
  message->set_schema(identifier.getSchemaName());

  return false;
}

} /* namespace drizzled */
