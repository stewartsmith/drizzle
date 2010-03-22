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

#include "config.h"
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/unireg.h>
#include "drizzled/sql_table.h"
#include "drizzled/global_charset_info.h"
#include "drizzled/message/statement_transform.h"

#include "drizzled/internal/my_sys.h"


/* For proto */
#include <string>
#include <fstream>
#include <fcntl.h>
#include <drizzled/message/schema.pb.h>
#include <drizzled/message/table.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <drizzled/table_proto.h>

using namespace std;

namespace drizzled {

static int fill_table_proto(message::Table &table_proto,
                            const std::string &table_name,
                            List<CreateField> &create_fields,
                            HA_CREATE_INFO *create_info,
                            uint32_t keys,
                            KEY *key_info)
{
  CreateField *field_arg;
  List_iterator<CreateField> it(create_fields);
  message::Table::TableOptions *table_options= table_proto.mutable_options();

  if (create_fields.elements > MAX_FIELDS)
  {
    my_error(ER_TOO_MANY_FIELDS, MYF(0), ER(ER_TOO_MANY_FIELDS));
    return(1);
  }

  assert(strcmp(table_proto.engine().name().c_str(),
		create_info->db_type->getName().c_str())==0);

  assert(table_proto.name() == table_name);

  int field_number= 0;
  bool use_existing_fields= table_proto.field_size() > 0;
  while ((field_arg= it++))
  {
    message::Table::Field *attribute;

    /* some (one) code path for CREATE TABLE fills the proto
       out more than the others, so we already have partially
       filled out Field messages */

    if (use_existing_fields)
      attribute= table_proto.mutable_field(field_number++);
    else
    {
      /* Other code paths still have to fill out the proto */
      attribute= table_proto.add_field();

      if (field_arg->flags & NOT_NULL_FLAG)
      {
        message::Table::Field::FieldConstraints *constraints;

        constraints= attribute->mutable_constraints();
        constraints->set_is_nullable(false);
      }

      attribute->set_name(field_arg->field_name);
    }

    assert((!(field_arg->flags & NOT_NULL_FLAG)) == attribute->constraints().is_nullable());
    assert(strcmp(attribute->name().c_str(), field_arg->field_name)==0);


    message::Table::Field::FieldType parser_type= attribute->type();

    attribute->set_type(message::internalFieldTypeToFieldProtoType(field_arg->sql_type));

    switch (attribute->type()) {
    default: /* Only deal with types that need extra information */
      break;
    case message::Table::Field::DOUBLE:
      {
        /*
         * For DOUBLE, we only add a specific scale and precision iff
         * the fixed decimal point has been specified...
         */
        if (field_arg->decimals != NOT_FIXED_DEC)
        {
          message::Table::Field::NumericFieldOptions *numeric_field_options;

          numeric_field_options= attribute->mutable_numeric_options();

          numeric_field_options->set_precision(field_arg->length);
          numeric_field_options->set_scale(field_arg->decimals);
        }
      }
      break;
    case message::Table::Field::VARCHAR:
      {
        message::Table::Field::StringFieldOptions *string_field_options;

        string_field_options= attribute->mutable_string_options();

        if (! use_existing_fields || string_field_options->length()==0)
          string_field_options->set_length(field_arg->length
                                           / field_arg->charset->mbmaxlen);
        else
          assert((uint32_t)string_field_options->length() == (uint32_t)(field_arg->length / field_arg->charset->mbmaxlen));

        if (! string_field_options->has_collation())
        {
          string_field_options->set_collation_id(field_arg->charset->number);
          string_field_options->set_collation(field_arg->charset->name);
        }
        break;
      }
    case message::Table::Field::DECIMAL:
      {
        message::Table::Field::NumericFieldOptions *numeric_field_options;

        numeric_field_options= attribute->mutable_numeric_options();
        /* This is magic, I hate magic numbers -Brian */
        numeric_field_options->set_precision(field_arg->length + ( field_arg->decimals ? -2 : -1));
        numeric_field_options->set_scale(field_arg->decimals);
        break;
      }
    case message::Table::Field::ENUM:
      {
        message::Table::Field::SetFieldOptions *set_field_options;

        assert(field_arg->interval);

        set_field_options= attribute->mutable_set_options();

        for (uint32_t pos= 0; pos < field_arg->interval->count; pos++)
        {
          const char *src= field_arg->interval->type_names[pos];

          set_field_options->add_field_value(src);
        }
	set_field_options->set_collation_id(field_arg->charset->number);
        set_field_options->set_collation(field_arg->charset->name);
        break;
      }
    case message::Table::Field::BLOB:
      {
        message::Table::Field::StringFieldOptions *string_field_options;

        string_field_options= attribute->mutable_string_options();
        string_field_options->set_collation_id(field_arg->charset->number);
        string_field_options->set_collation(field_arg->charset->name);
      }

      break;
    }

    assert (!use_existing_fields || parser_type == attribute->type());

#ifdef NOTDONE
    field_constraints= attribute->mutable_constraints();
    constraints->set_is_nullable(field_arg->def->null_value);
#endif

    switch(field_arg->column_format())
    {
    case COLUMN_FORMAT_TYPE_NOT_USED:
      break;
    case COLUMN_FORMAT_TYPE_DEFAULT:
      attribute->set_format(message::Table::Field::DefaultFormat);
      break;
    case COLUMN_FORMAT_TYPE_FIXED:
      attribute->set_format(message::Table::Field::FixedFormat);
      break;
    case COLUMN_FORMAT_TYPE_DYNAMIC:
      attribute->set_format(message::Table::Field::DynamicFormat);
      break;
    default:
      assert(0); /* Tell us, since this shouldn't happend */
    }

    if (field_arg->comment.length)
    {
      uint32_t tmp_len;
      tmp_len= system_charset_info->cset->charpos(system_charset_info,
						  field_arg->comment.str,
						  field_arg->comment.str +
						  field_arg->comment.length,
						  COLUMN_COMMENT_MAXLEN);

      if (tmp_len < field_arg->comment.length)
      {
	my_error(ER_WRONG_STRING_LENGTH, MYF(0),
		 field_arg->comment.str,"COLUMN COMMENT",
		 (uint32_t) COLUMN_COMMENT_MAXLEN);
	return(1);
      }

      if (! use_existing_fields)
        attribute->set_comment(field_arg->comment.str);

      assert(strcmp(attribute->comment().c_str(), field_arg->comment.str)==0);
    }

    if (field_arg->unireg_check == Field::NEXT_NUMBER)
    {
      message::Table::Field::NumericFieldOptions *field_options;
      field_options= attribute->mutable_numeric_options();
      field_options->set_is_autoincrement(true);
    }

    if (field_arg->unireg_check == Field::TIMESTAMP_DN_FIELD
       || field_arg->unireg_check == Field::TIMESTAMP_DNUN_FIELD)
    {
      message::Table::Field::FieldOptions *field_options;
      field_options= attribute->mutable_options();
      field_options->set_default_value("NOW()");
    }

    if (field_arg->unireg_check == Field::TIMESTAMP_UN_FIELD
       || field_arg->unireg_check == Field::TIMESTAMP_DNUN_FIELD)
    {
      message::Table::Field::FieldOptions *field_options;
      field_options= attribute->mutable_options();
      field_options->set_update_value("NOW()");
    }

    if (field_arg->def == NULL  && attribute->constraints().is_nullable())
    {
      message::Table::Field::FieldOptions *field_options;
      field_options= attribute->mutable_options();

      field_options->set_default_null(true);
    }
    if (field_arg->def)
    {
      message::Table::Field::FieldOptions *field_options;
      field_options= attribute->mutable_options();
 
      if (field_arg->def->is_null())
      {
	field_options->set_default_null(true);
      }
      else
      {
	String d;
	String *default_value= field_arg->def->val_str(&d);

	assert(default_value);

	if ((field_arg->sql_type==DRIZZLE_TYPE_VARCHAR
	   || field_arg->sql_type==DRIZZLE_TYPE_BLOB)
	   && ((field_arg->length / field_arg->charset->mbmaxlen)
	   < default_value->length()))
	{
	  my_error(ER_INVALID_DEFAULT, MYF(0), field_arg->field_name);
	  return 1;
	}

	if ((field_arg->sql_type==DRIZZLE_TYPE_VARCHAR
	    && field_arg->charset==&my_charset_bin)
	   || (field_arg->sql_type==DRIZZLE_TYPE_BLOB
	    && field_arg->charset==&my_charset_bin))
	{
	  string bin_default;
	  bin_default.assign(default_value->c_ptr(),
			     default_value->length());
	  field_options->set_default_bin_value(bin_default);
	}
	else
	{
	  field_options->set_default_value(default_value->c_ptr());
	}
      }
    }

    assert(field_arg->unireg_check == Field::NONE
	   || field_arg->unireg_check == Field::NEXT_NUMBER
	   || field_arg->unireg_check == Field::TIMESTAMP_DN_FIELD
	   || field_arg->unireg_check == Field::TIMESTAMP_UN_FIELD
	   || field_arg->unireg_check == Field::TIMESTAMP_DNUN_FIELD);

  }

  assert(! use_existing_fields || (field_number == table_proto.field_size()));

  if (create_info->table_options & HA_OPTION_PACK_RECORD)
    table_options->set_pack_record(true);

  if (table_options->has_comment())
  {
    uint32_t tmp_len;
    tmp_len= system_charset_info->cset->charpos(system_charset_info,
                                                table_options->comment().c_str(),
                                                table_options->comment().c_str() +
                                                table_options->comment().length(),
                                                TABLE_COMMENT_MAXLEN);

    if (tmp_len < table_options->comment().length())
    {
      my_error(ER_WRONG_STRING_LENGTH, MYF(0),
               table_options->comment().c_str(),"Table COMMENT",
               (uint32_t) TABLE_COMMENT_MAXLEN);
      return(1);
    }
  }

  if (create_info->default_table_charset)
  {
    table_options->set_collation_id(
			       create_info->default_table_charset->number);
    table_options->set_collation(create_info->default_table_charset->name);
  }

  if (create_info->auto_increment_value)
    table_options->set_auto_increment_value(create_info->auto_increment_value);

  for (uint32_t i= 0; i < keys; i++)
  {
    message::Table::Index *idx;

    idx= table_proto.add_indexes();

    assert(test(key_info[i].flags & HA_USES_COMMENT) ==
           (key_info[i].comment.length > 0));

    idx->set_name(key_info[i].name);

    idx->set_key_length(key_info[i].key_length);

    if (is_primary_key_name(key_info[i].name))
      idx->set_is_primary(true);
    else
      idx->set_is_primary(false);

    switch(key_info[i].algorithm)
    {
    case HA_KEY_ALG_HASH:
      idx->set_type(message::Table::Index::HASH);
      break;

    case HA_KEY_ALG_BTREE:
      idx->set_type(message::Table::Index::BTREE);
      break;

    case HA_KEY_ALG_UNDEF:
      idx->set_type(message::Table::Index::UNKNOWN_INDEX);
      break;

    default:
      abort(); /* Somebody's brain broke. haven't added index type to proto */
    }

    if (key_info[i].flags & HA_NOSAME)
      idx->set_is_unique(true);
    else
      idx->set_is_unique(false);

    message::Table::Index::IndexOptions *index_options= idx->mutable_options();

    if (key_info[i].flags & HA_USES_BLOCK_SIZE)
      index_options->set_key_block_size(key_info[i].block_size);

    if (key_info[i].flags & HA_PACK_KEY)
      index_options->set_pack_key(true);

    if (key_info[i].flags & HA_BINARY_PACK_KEY)
      index_options->set_binary_pack_key(true);

    if (key_info[i].flags & HA_VAR_LENGTH_PART)
      index_options->set_var_length_key(true);

    if (key_info[i].flags & HA_NULL_PART_KEY)
      index_options->set_null_part_key(true);

    if (key_info[i].flags & HA_KEY_HAS_PART_KEY_SEG)
      index_options->set_has_partial_segments(true);

    if (key_info[i].flags & HA_GENERATED_KEY)
      index_options->set_auto_generated_key(true);

    if (key_info[i].flags & HA_USES_COMMENT)
    {
      uint32_t tmp_len;
      tmp_len= system_charset_info->cset->charpos(system_charset_info,
						  key_info[i].comment.str,
						  key_info[i].comment.str +
						  key_info[i].comment.length,
						  TABLE_COMMENT_MAXLEN);

      if (tmp_len < key_info[i].comment.length)
      {
	my_error(ER_WRONG_STRING_LENGTH, MYF(0),
		 key_info[i].comment.str,"Index COMMENT",
		 (uint32_t) TABLE_COMMENT_MAXLEN);
	return(1);
      }

      idx->set_comment(key_info[i].comment.str);
    }
    if (key_info[i].flags & 
        ~(HA_NOSAME | HA_PACK_KEY | HA_USES_BLOCK_SIZE | 
          HA_BINARY_PACK_KEY | HA_VAR_LENGTH_PART | HA_NULL_PART_KEY | 
          HA_KEY_HAS_PART_KEY_SEG | HA_GENERATED_KEY | HA_USES_COMMENT))
      abort(); // Invalid (unknown) index flag.

    for(unsigned int j=0; j< key_info[i].key_parts; j++)
    {
      message::Table::Index::IndexPart *idxpart;

      idxpart= idx->add_index_part();

      idxpart->set_fieldnr(key_info[i].key_part[j].fieldnr);

      idxpart->set_compare_length(key_info[i].key_part[j].length);

      idxpart->set_key_type(key_info[i].key_part[j].key_type);

    }
  }

  return 0;
}

int rename_table_proto_file(const char *from, const char* to)
{
  string from_path(from);
  string to_path(to);
  string file_ext = ".dfe";

  from_path.append(file_ext);
  to_path.append(file_ext);

  return internal::my_rename(from_path.c_str(),to_path.c_str(),MYF(MY_WME));
}

int delete_table_proto_file(const char *file_name)
{
  string new_path(file_name);
  string file_ext = ".dfe";

  new_path.append(file_ext);
  return internal::my_delete(new_path.c_str(), MYF(0));
}

/*
  Create a table definition proto file and the tables

  SYNOPSIS
    rea_create_table()
    session			Thread handler
    path		Name of file (including database, without .frm)
    db			Data base name
    table_name		Table name
    create_info		create info parameters
    create_fields	Fields to create
    keys		number of keys to create
    key_info		Keys to create

  RETURN
    0  ok
    1  error
*/

bool rea_create_table(Session *session,
                      TableIdentifier &identifier,
                      message::Table &table_proto,
                      HA_CREATE_INFO *create_info,
                      List<CreateField> &create_fields,
                      uint32_t keys, KEY *key_info)
{
  if (fill_table_proto(table_proto, identifier.getTableName(), create_fields, create_info,
                       keys, key_info))
    return false;

  if (plugin::StorageEngine::createTable(*session,
                                         identifier,
                                         false, table_proto))
  {
    return false;
  }

  return true;

} /* rea_create_table */

} /* namespace drizzled */

