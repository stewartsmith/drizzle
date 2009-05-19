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

#include <drizzled/server_includes.h>
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/unireg.h>

/* For proto */
#include <string>
#include <fstream>
#include <drizzled/message/schema.pb.h>
#include <drizzled/message/table.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
using namespace std;

int drizzle_read_table_proto(const char* path, drizzled::message::Table* table)
{
  int fd= open(path, O_RDONLY);

  if(fd==-1)
    return errno;

  google::protobuf::io::ZeroCopyInputStream* input=
    new google::protobuf::io::FileInputStream(fd);

  if (!table->ParseFromZeroCopyStream(input))
  {
    delete input;
    close(fd);
    return -1;
  }

  delete input;
  close(fd);
  return 0;
}

static int fill_table_proto(drizzled::message::Table *table_proto,
			    const char *table_name,
			    List<Create_field> &create_fields,
			    HA_CREATE_INFO *create_info,
			    uint32_t keys,
			    KEY *key_info)
{
  Create_field *field_arg;
  List_iterator<Create_field> it(create_fields);
  drizzled::message::Table::StorageEngine *engine= table_proto->mutable_engine();
  drizzled::message::Table::TableOptions *table_options= table_proto->mutable_options();

  if (create_fields.elements > MAX_FIELDS)
  {
    my_error(ER_TOO_MANY_FIELDS, MYF(0), ER(ER_TOO_MANY_FIELDS));
    return(1);
  }

  engine->set_name(create_info->db_type->getName());

  assert(strcmp(table_proto->name().c_str(),table_name)==0);
  table_proto->set_type(drizzled::message::Table::STANDARD);

  while ((field_arg= it++))
  {
    drizzled::message::Table::Field *attribute;

    attribute= table_proto->add_field();
    attribute->set_name(field_arg->field_name);

    attribute->set_pack_flag(field_arg->pack_flag); /* TODO: MUST DIE */

    if(f_maybe_null(field_arg->pack_flag))
    {
      drizzled::message::Table::Field::FieldConstraints *constraints;

      constraints= attribute->mutable_constraints();
      constraints->set_is_nullable(true);
    }

    switch (field_arg->sql_type) {
    case DRIZZLE_TYPE_TINY:
      attribute->set_type(drizzled::message::Table::Field::TINYINT);
      break;
    case DRIZZLE_TYPE_LONG:
      attribute->set_type(drizzled::message::Table::Field::INTEGER);
      break;
    case DRIZZLE_TYPE_DOUBLE:
      attribute->set_type(drizzled::message::Table::Field::DOUBLE);
      break;
    case DRIZZLE_TYPE_NULL  :
      assert(1); /* Not a user definable type */
    case DRIZZLE_TYPE_TIMESTAMP:
      attribute->set_type(drizzled::message::Table::Field::TIMESTAMP);
      break;
    case DRIZZLE_TYPE_LONGLONG:
      attribute->set_type(drizzled::message::Table::Field::BIGINT);
      break;
    case DRIZZLE_TYPE_DATETIME:
      attribute->set_type(drizzled::message::Table::Field::DATETIME);
      break;
    case DRIZZLE_TYPE_DATE:
      attribute->set_type(drizzled::message::Table::Field::DATE);
      break;
    case DRIZZLE_TYPE_VARCHAR:
      {
        drizzled::message::Table::Field::StringFieldOptions *string_field_options;

        string_field_options= attribute->mutable_string_options();
        attribute->set_type(drizzled::message::Table::Field::VARCHAR);
        string_field_options->set_length(field_arg->length
					 / field_arg->charset->mbmaxlen);
        string_field_options->set_collation_id(field_arg->charset->number);
        string_field_options->set_collation(field_arg->charset->name);

        break;
      }
    case DRIZZLE_TYPE_NEWDECIMAL:
      {
        drizzled::message::Table::Field::NumericFieldOptions *numeric_field_options;

        attribute->set_type(drizzled::message::Table::Field::DECIMAL);
        numeric_field_options= attribute->mutable_numeric_options();
        /* This is magic, I hate magic numbers -Brian */
        numeric_field_options->set_precision(field_arg->length + ( field_arg->decimals ? -2 : -1));
        numeric_field_options->set_scale(field_arg->decimals);
        break;
      }
    case DRIZZLE_TYPE_ENUM:
      {
        drizzled::message::Table::Field::SetFieldOptions *set_field_options;

        assert(field_arg->interval);

        attribute->set_type(drizzled::message::Table::Field::ENUM);
        set_field_options= attribute->mutable_set_options();

        for (uint32_t pos= 0; pos < field_arg->interval->count; pos++)
        {
          const char *src= field_arg->interval->type_names[pos];

          set_field_options->add_field_value(src);
        }
        set_field_options->set_count_elements(set_field_options->field_value_size());
	set_field_options->set_collation_id(field_arg->charset->number);
        set_field_options->set_collation(field_arg->charset->name);
        break;
      }
    case DRIZZLE_TYPE_BLOB:
      {
	attribute->set_type(drizzled::message::Table::Field::BLOB);

        drizzled::message::Table::Field::StringFieldOptions *string_field_options;

        string_field_options= attribute->mutable_string_options();
        string_field_options->set_collation_id(field_arg->charset->number);
        string_field_options->set_collation(field_arg->charset->name);
      }

      break;
    default:
      assert(0); /* Tell us, since this shouldn't happend */
    }

#ifdef NOTDONE
    field_constraints= attribute->mutable_constraints();
    constraints->set_is_nullable(field_arg->def->null_value);
#endif

    switch(field_arg->column_format())
    {
    case COLUMN_FORMAT_TYPE_NOT_USED:
      break;
    case COLUMN_FORMAT_TYPE_DEFAULT:
      attribute->set_format(drizzled::message::Table::Field::DefaultFormat);
      break;
    case COLUMN_FORMAT_TYPE_FIXED:
      attribute->set_format(drizzled::message::Table::Field::FixedFormat);
      break;
    case COLUMN_FORMAT_TYPE_DYNAMIC:
      attribute->set_format(drizzled::message::Table::Field::DynamicFormat);
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

      attribute->set_comment(field_arg->comment.str);
    }

    if(field_arg->unireg_check == Field::NEXT_NUMBER)
    {
      drizzled::message::Table::Field::NumericFieldOptions *field_options;
      field_options= attribute->mutable_numeric_options();
      field_options->set_is_autoincrement(true);
    }

    if(field_arg->unireg_check == Field::TIMESTAMP_DN_FIELD
       || field_arg->unireg_check == Field::TIMESTAMP_DNUN_FIELD)
    {
      drizzled::message::Table::Field::FieldOptions *field_options;
      field_options= attribute->mutable_options();
      field_options->set_default_value("NOW()");
    }

    if(field_arg->unireg_check == Field::TIMESTAMP_UN_FIELD
       || field_arg->unireg_check == Field::TIMESTAMP_DNUN_FIELD)
    {
      drizzled::message::Table::Field::FieldOptions *field_options;
      field_options= attribute->mutable_options();
      field_options->set_update_value("NOW()");
    }

    if(field_arg->def)
    {
      drizzled::message::Table::Field::FieldOptions *field_options;
      field_options= attribute->mutable_options();

      if(field_arg->def->is_null())
      {
	field_options->set_default_null(true);
      }
      else
      {
	String d;
	String *default_value= field_arg->def->val_str(&d);

	assert(default_value);

	if((field_arg->sql_type==DRIZZLE_TYPE_VARCHAR
	   || field_arg->sql_type==DRIZZLE_TYPE_BLOB)
	   && ((field_arg->length / field_arg->charset->mbmaxlen)
	   < default_value->length()))
	{
	  my_error(ER_INVALID_DEFAULT, MYF(0), field_arg->field_name);
	  return 1;
	}

	if((field_arg->sql_type==DRIZZLE_TYPE_VARCHAR
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

    {
      drizzled::message::Table::Field::FieldOptions *field_options;
      field_options= attribute->mutable_options();

      field_options->set_length(field_arg->length);
    }

    assert(field_arg->unireg_check == Field::NONE
	   || field_arg->unireg_check == Field::NEXT_NUMBER
	   || field_arg->unireg_check == Field::TIMESTAMP_DN_FIELD
	   || field_arg->unireg_check == Field::TIMESTAMP_UN_FIELD
	   || field_arg->unireg_check == Field::TIMESTAMP_DNUN_FIELD);

  }

  if (create_info->used_fields & HA_CREATE_USED_PACK_KEYS)
  {
    if(create_info->table_options & HA_OPTION_PACK_KEYS)
      table_options->set_pack_keys(true);
    else if(create_info->table_options & HA_OPTION_NO_PACK_KEYS)
      table_options->set_pack_keys(false);
  }
  else
    if(create_info->table_options & HA_OPTION_PACK_KEYS)
      table_options->set_pack_keys(true);


  if (create_info->used_fields & HA_CREATE_USED_CHECKSUM)
  {
    assert(create_info->table_options & (HA_OPTION_CHECKSUM | HA_OPTION_NO_CHECKSUM));

    if(create_info->table_options & HA_OPTION_CHECKSUM)
      table_options->set_checksum(true);
    else
      table_options->set_checksum(false);
  }
  else if(create_info->table_options & HA_OPTION_CHECKSUM)
    table_options->set_checksum(true);


  if (create_info->used_fields & HA_CREATE_USED_PAGE_CHECKSUM)
  {
    if (create_info->page_checksum == HA_CHOICE_YES)
      table_options->set_page_checksum(true);
    else if (create_info->page_checksum == HA_CHOICE_NO)
      table_options->set_page_checksum(false);
  }
  else if (create_info->page_checksum == HA_CHOICE_YES)
    table_options->set_page_checksum(true);


  if (create_info->used_fields & HA_CREATE_USED_DELAY_KEY_WRITE)
  {
    if(create_info->table_options & HA_OPTION_DELAY_KEY_WRITE)
      table_options->set_delay_key_write(true);
    else if(create_info->table_options & HA_OPTION_NO_DELAY_KEY_WRITE)
      table_options->set_delay_key_write(false);
  }
  else if(create_info->table_options & HA_OPTION_DELAY_KEY_WRITE)
    table_options->set_delay_key_write(true);


  switch(create_info->row_type)
  {
  case ROW_TYPE_DEFAULT:
    table_options->set_row_type(drizzled::message::Table::TableOptions::ROW_TYPE_DEFAULT);
    break;
  case ROW_TYPE_FIXED:
    table_options->set_row_type(drizzled::message::Table::TableOptions::ROW_TYPE_FIXED);
    break;
  case ROW_TYPE_DYNAMIC:
    table_options->set_row_type(drizzled::message::Table::TableOptions::ROW_TYPE_DYNAMIC);
    break;
  case ROW_TYPE_COMPRESSED:
    table_options->set_row_type(drizzled::message::Table::TableOptions::ROW_TYPE_COMPRESSED);
    break;
  case ROW_TYPE_REDUNDANT:
    table_options->set_row_type(drizzled::message::Table::TableOptions::ROW_TYPE_REDUNDANT);
    break;
  case ROW_TYPE_COMPACT:
    table_options->set_row_type(drizzled::message::Table::TableOptions::ROW_TYPE_COMPACT);
    break;
  case ROW_TYPE_PAGE:
    table_options->set_row_type(drizzled::message::Table::TableOptions::ROW_TYPE_PAGE);
    break;
  default:
    abort();
  }

  table_options->set_pack_record(create_info->table_options
				 & HA_OPTION_PACK_RECORD);

  if (create_info->comment.length)
  {
    uint32_t tmp_len;
    tmp_len= system_charset_info->cset->charpos(system_charset_info,
						create_info->comment.str,
						create_info->comment.str +
						create_info->comment.length,
						TABLE_COMMENT_MAXLEN);

    if (tmp_len < create_info->comment.length)
    {
      my_error(ER_WRONG_STRING_LENGTH, MYF(0),
	       create_info->comment.str,"Table COMMENT",
	       (uint32_t) TABLE_COMMENT_MAXLEN);
      return(1);
    }

    table_options->set_comment(create_info->comment.str);
  }
  if (create_info->default_table_charset)
  {
    table_options->set_collation_id(
			       create_info->default_table_charset->number);
    table_options->set_collation(create_info->default_table_charset->name);
  }

  if (create_info->connect_string.length)
    table_options->set_connect_string(create_info->connect_string.str);

  if (create_info->data_file_name)
    table_options->set_data_file_name(create_info->data_file_name);

  if (create_info->index_file_name)
    table_options->set_index_file_name(create_info->index_file_name);

  if (create_info->max_rows)
    table_options->set_max_rows(create_info->max_rows);

  if (create_info->min_rows)
    table_options->set_min_rows(create_info->min_rows);

  if (create_info->auto_increment_value)
    table_options->set_auto_increment_value(create_info->auto_increment_value);

  if (create_info->avg_row_length)
    table_options->set_avg_row_length(create_info->avg_row_length);

  if (create_info->key_block_size)
    table_options->set_key_block_size(create_info->key_block_size);

  if (create_info->block_size)
    table_options->set_block_size(create_info->block_size);

  for (unsigned int i= 0; i < keys; i++)
  {
    drizzled::message::Table::Index *idx;

    idx= table_proto->add_indexes();

    assert(test(key_info[i].flags & HA_USES_COMMENT) ==
           (key_info[i].comment.length > 0));

    idx->set_name(key_info[i].name);

    idx->set_key_length(key_info[i].key_length);

    if(is_primary_key_name(key_info[i].name))
      idx->set_is_primary(true);
    else
      idx->set_is_primary(false);

    switch(key_info[i].algorithm)
    {
    case HA_KEY_ALG_HASH:
      idx->set_type(drizzled::message::Table::Index::HASH);
      break;

    case HA_KEY_ALG_BTREE:
      idx->set_type(drizzled::message::Table::Index::BTREE);
      break;

    case HA_KEY_ALG_RTREE:
      idx->set_type(drizzled::message::Table::Index::RTREE);
    case HA_KEY_ALG_FULLTEXT:
      idx->set_type(drizzled::message::Table::Index::FULLTEXT);
    case HA_KEY_ALG_UNDEF:
      idx->set_type(drizzled::message::Table::Index::UNKNOWN_INDEX);
      break;

    default:
      abort(); /* Somebody's brain broke. haven't added index type to proto */
    }

    if (key_info[i].flags & HA_NOSAME)
      idx->set_is_unique(true);
    else
      idx->set_is_unique(false);

    drizzled::message::Table::Index::IndexOptions *index_options= idx->mutable_options();

    if(key_info[i].flags & HA_USES_BLOCK_SIZE)
      index_options->set_key_block_size(key_info[i].block_size);

    if(key_info[i].flags & HA_PACK_KEY)
      index_options->set_pack_key(true);

    if(key_info[i].flags & HA_BINARY_PACK_KEY)
      index_options->set_binary_pack_key(true);

    if(key_info[i].flags & HA_VAR_LENGTH_PART)
      index_options->set_var_length_key(true);

    if(key_info[i].flags & HA_NULL_PART_KEY)
      index_options->set_null_part_key(true);

    if(key_info[i].flags & HA_KEY_HAS_PART_KEY_SEG)
      index_options->set_has_partial_segments(true);

    if(key_info[i].flags & HA_GENERATED_KEY)
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
    if(key_info[i].flags & ~(HA_NOSAME | HA_PACK_KEY | HA_USES_BLOCK_SIZE | HA_BINARY_PACK_KEY | HA_VAR_LENGTH_PART | HA_NULL_PART_KEY | HA_KEY_HAS_PART_KEY_SEG | HA_GENERATED_KEY | HA_USES_COMMENT))
      abort(); // Invalid (unknown) index flag.

    for(unsigned int j=0; j< key_info[i].key_parts; j++)
    {
      drizzled::message::Table::Index::IndexPart *idxpart;

      idxpart= idx->add_index_part();

      idxpart->set_fieldnr(key_info[i].key_part[j].fieldnr);

      idxpart->set_compare_length(key_info[i].key_part[j].length);

      idxpart->set_key_type(key_info[i].key_part[j].key_type);

    }
  }

  return 0;
}

int copy_table_proto_file(const char *from, const char* to)
{
  string dfesrc(from);
  string dfedst(to);
  string file_ext = ".dfe";

  dfesrc.append(file_ext);
  dfedst.append(file_ext);

  return my_copy(dfesrc.c_str(), dfedst.c_str(),
		 MYF(MY_DONT_OVERWRITE_FILE));
}

int rename_table_proto_file(const char *from, const char* to)
{
  string from_path(from);
  string to_path(to);
  string file_ext = ".dfe";

  from_path.append(file_ext);
  to_path.append(file_ext);

  return my_rename(from_path.c_str(),to_path.c_str(),MYF(MY_WME));
}

int delete_table_proto_file(const char *file_name)
{
  string new_path(file_name);
  string file_ext = ".dfe";

  new_path.append(file_ext);
  return my_delete(new_path.c_str(), MYF(0));
}

int table_proto_exists(const char *path)
{
  string proto_path(path);
  string file_ext(".dfe");
  proto_path.append(file_ext);

  int error= access(proto_path.c_str(), F_OK);

  if(error==0)
    return EEXIST;
  else
    return errno;
}

static int create_table_proto_file(const char *file_name,
				   const char *db,
				   const char *table_name,
				   drizzled::message::Table *table_proto,
				   HA_CREATE_INFO *create_info,
				   List<Create_field> &create_fields,
				   uint32_t keys,
				   KEY *key_info)
{
  string new_path(file_name);
  string file_ext = ".dfe";

  if(fill_table_proto(table_proto, table_name, create_fields, create_info,
		      keys, key_info))
    return -1;

  new_path.append(file_ext);

  int fd= open(new_path.c_str(), O_RDWR|O_CREAT|O_TRUNC, my_umask);

  if(fd==-1)
  {
    if(errno==ENOENT)
      my_error(ER_BAD_DB_ERROR,MYF(0),db);
    else
      my_error(ER_CANT_CREATE_TABLE,MYF(0),table_name,errno);
    return errno;
  }

  google::protobuf::io::ZeroCopyOutputStream* output=
    new google::protobuf::io::FileOutputStream(fd);

  if (!table_proto->SerializeToZeroCopyStream(output))
  {
    delete output;
    close(fd);
    return errno;
  }

  delete output;
  close(fd);
  return 0;
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
    file		Handler to use
    is_like             is true for mysql_create_like_schema_frm

  RETURN
    0  ok
    1  error
*/

int rea_create_table(Session *session, const char *path,
                     const char *db, const char *table_name,
		     drizzled::message::Table *table_proto,
                     HA_CREATE_INFO *create_info,
                     List<Create_field> &create_fields,
                     uint32_t keys, KEY *key_info, handler *file,
                     bool is_like)
{
  /* Proto will blow up unless we give a name */
  assert(table_name);

  /* For is_like we return once the file has been created */
  if (is_like)
  {
    if (create_table_proto_file(path, db, table_name, table_proto,
				create_info,
                                create_fields, keys, key_info)!=0)
      return 1;

    return 0;
  }
  /* Here we need to build the full frm from the path */
  else
  {
    if (create_table_proto_file(path, db, table_name, table_proto,
				create_info,
                                create_fields, keys, key_info))
      return 1;
  }

  // Make sure mysql_create_frm din't remove extension
  if (session->variables.keep_files_on_create)
    create_info->options|= HA_CREATE_KEEP_FILES;
  if (file->ha_create_handler_files(path, NULL, CHF_CREATE_FLAG, create_info))
    goto err_handler;
  if (ha_create_table(session, path, db, table_name,
                      create_info,0))
    goto err_handler;
  return 0;

err_handler:
  file->ha_create_handler_files(path, NULL, CHF_DELETE_FLAG, create_info);

  delete_table_proto_file(path);

  return 1;
} /* rea_create_table */
