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


/*
  Functions to create a unireg form-file from a FIELD and a fieldname-fieldinfo
  struct.
  In the following functions FIELD * is an ordinary field-structure with
  the following exeptions:
    sc_length,typepos,row,kol,dtype,regnr and field need not to be set.
    str is a (long) to record position where 0 is the first position.
*/

#include <drizzled/server_includes.h>
#include <drizzled/error.h>
#include <drizzled/virtual_column_info.h>
#include <drizzled/session.h>
#include <drizzled/unireg.h>

/* For proto */
#include <string>
#include <fstream>
#include <drizzled/serialize/serialize.h>
#include <drizzled/serialize/table.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
using namespace std;

#define FCOMP			17		/* Bytes for a packed field */

static unsigned char * pack_screens(List<Create_field> &create_fields,
                                    uint32_t *info_length, uint32_t *screens,
                                    bool small_file);
static uint32_t pack_keys(unsigned char *keybuff,uint32_t key_count,
                          KEY *key_info, ulong data_offset);
static bool pack_header(unsigned char *forminfo,
                        List<Create_field> &create_fields,
                        uint32_t info_length, uint32_t screens,
                        ulong data_offset, handler *file);
static uint32_t get_interval_id(uint32_t *int_count,
                                List<Create_field> &create_fields,
                                Create_field *last_field);
static bool pack_fields(File file, List<Create_field> &create_fields,
                        ulong data_offset);
static bool make_empty_rec(Session *session, int file,
                           enum legacy_db_type table_type,
                           uint32_t table_options,
                           List<Create_field> &create_fields,
                           uint32_t reclength, ulong data_offset,
                           handler *handler);

/**
  An interceptor to hijack ER_TOO_MANY_FIELDS error from
  pack_screens and retry again without UNIREG screens.

  XXX: what is a UNIREG  screen?
*/

struct Pack_header_error_handler: public Internal_error_handler
{
  virtual bool handle_error(uint32_t sql_errno,
                            const char *message,
                            DRIZZLE_ERROR::enum_warning_level level,
                            Session *session);
  bool is_handled;
  Pack_header_error_handler() :is_handled(false) {}
};


bool
Pack_header_error_handler::
handle_error(uint32_t sql_errno,
             const char * /* message */,
             DRIZZLE_ERROR::enum_warning_level /* level */,
             Session * /* session */)
{
  is_handled= (sql_errno == ER_TOO_MANY_FIELDS);
  return is_handled;
}

int drizzle_read_table_proto(const char* path, drizzle::Table* table)
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

/*
  Create a frm (table definition) file

  SYNOPSIS
    mysql_create_frm()
    session			Thread handler
    file_name		Path for file (including database and .frm)
    db                  Name of database
    table               Name of table
    create_info		create info parameters
    create_fields	Fields to create
    keys		number of keys to create
    key_info		Keys to create
    db_file		Handler to use. May be zero, in which case we use
			create_info->db_type
  RETURN
    0  ok
    1  error
*/

bool mysql_create_frm(Session *session, const char *file_name,
                      const char *db, const char *table,
		      HA_CREATE_INFO *create_info,
		      List<Create_field> &create_fields,
		      uint32_t keys, KEY *key_info,
		      handler *db_file)
{
  LEX_STRING str_db_type;
  uint32_t reclength, info_length, screens, key_info_length, maxlength, tmp_len;
  ulong key_buff_length;
  File file;
  ulong filepos, data_offset;
  unsigned char fileinfo[64],forminfo[288],*keybuff;
  TYPELIB formnames;
  unsigned char *screen_buff;
  char buff[128];
  const uint32_t format_section_header_size= 8;
  uint32_t format_section_len;
  Pack_header_error_handler pack_header_error_handler;
  int error;

  assert(*fn_rext((char*)file_name)); // Check .frm extension
  formnames.type_names=0;
  if (!(screen_buff=pack_screens(create_fields,&info_length,&screens,0)))
    return(1);
  assert(db_file != NULL);

 /* If fixed row records, we need one bit to check for deleted rows */
  if (!(create_info->table_options & HA_OPTION_PACK_RECORD))
    create_info->null_bits++;
  data_offset= (create_info->null_bits + 7) / 8;

  session->push_internal_handler(&pack_header_error_handler);

  error= pack_header(forminfo,
                     create_fields,info_length,
                     screens,
                     data_offset, db_file);

  session->pop_internal_handler();

  if (error)
  {
    free(screen_buff);
    if (! pack_header_error_handler.is_handled)
      return(1);

    // Try again without UNIREG screens (to get more columns)
    if (!(screen_buff=pack_screens(create_fields,&info_length,&screens,1)))
      return(1);
    if (pack_header(forminfo,
                    create_fields,info_length,
		    screens, data_offset, db_file))
    {
      free(screen_buff);
      return(1);
    }
  }
  reclength=uint2korr(forminfo+266);

  /* Calculate extra data segment length */
  str_db_type.str= (char *) ha_resolve_storage_engine_name(create_info->db_type);
  str_db_type.length= strlen(str_db_type.str);
  /* str_db_type */
  create_info->extra_size= (2 + str_db_type.length +
                            2 + create_info->connect_string.length);
  /*
    Partition:
      Length of partition info = 4 byte
      Potential NULL byte at end of partition info string = 1 byte
      Indicator if auto-partitioned table = 1 byte
      => Total 6 byte
  */
  create_info->extra_size+= 6;

  /* Add space for storage type and field format array of fields */
  format_section_len=
    format_section_header_size + 1 + create_fields.elements;
  create_info->extra_size+= format_section_len;

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
    free(screen_buff);
    return(1);
  }

  //if table comment is larger than 180 bytes, store into extra segment.
  if (create_info->comment.length > 180)
  {
    forminfo[46]=255;
    create_info->extra_size+= 2 + create_info->comment.length;
  }
  else{
    strncpy((char*) forminfo+47, create_info->comment.str ?
            create_info->comment.str : "", create_info->comment.length);
    forminfo[46]=(unsigned char) create_info->comment.length;
#ifdef EXTRA_DEBUG
    /*
      EXTRA_DEBUG causes strmake() to initialize its buffer behind the
      payload with a magic value to detect wrong buffer-sizes. We
      explicitly zero that segment again.
    */
    memset(forminfo+47 + forminfo[46], 0, 61 - forminfo[46]);
#endif
  }

  if ((file=create_frm(session, file_name, db, table, reclength, fileinfo,
		       create_info, keys, key_info)) < 0)
  {
    free(screen_buff);
    return(1);
  }

  key_buff_length= uint4korr(fileinfo+47);
  keybuff=(unsigned char*) malloc(key_buff_length);
  key_info_length= pack_keys(keybuff, keys, key_info, data_offset);
  get_form_pos(file,fileinfo,&formnames);
  if (!(filepos=make_new_entry(file,fileinfo,&formnames,"")))
    goto err;
  maxlength=(uint32_t) next_io_size((ulong) (uint2korr(forminfo)+1000));
  int2store(forminfo+2,maxlength);
  int4store(fileinfo+10,(ulong) (filepos+maxlength));
  fileinfo[26]= (unsigned char) test((create_info->max_rows == 1) &&
			     (create_info->min_rows == 1) && (keys == 0));
  int2store(fileinfo+28,key_info_length);


  int2store(fileinfo+59, 0); /* was: extra_rec_buf_length */

  if (pwrite(file, fileinfo, 64, 0L) == 0 ||
      pwrite(file, keybuff, key_info_length, (ulong) uint2korr(fileinfo+6)) == 0)
    goto err;
  lseek(file, (off_t)(uint2korr(fileinfo+6) + key_buff_length), SEEK_SET);
  if (make_empty_rec(session,file,ha_legacy_type(create_info->db_type),
                     create_info->table_options,
		     create_fields,reclength, data_offset, db_file))
    goto err;

  int2store(buff, create_info->connect_string.length);
  if (my_write(file, (const unsigned char*)buff, 2, MYF(MY_NABP)) ||
      my_write(file, (const unsigned char*)create_info->connect_string.str,
               create_info->connect_string.length, MYF(MY_NABP)))
      goto err;

  int2store(buff, str_db_type.length);
  if (my_write(file, (const unsigned char*)buff, 2, MYF(MY_NABP)) ||
      my_write(file, (const unsigned char*)str_db_type.str,
               str_db_type.length, MYF(MY_NABP)))
    goto err;

  {
    memset(buff, 0, 6);
    if (my_write(file, (unsigned char*) buff, 6, MYF_RW))
      goto err;
  }

  if (forminfo[46] == (unsigned char)255)
  {
    unsigned char comment_length_buff[2];
    int2store(comment_length_buff,create_info->comment.length);
    if (my_write(file, comment_length_buff, 2, MYF(MY_NABP)) ||
        my_write(file, (unsigned char*)create_info->comment.str,
                  create_info->comment.length, MYF(MY_NABP)))
      goto err;
  }

  /* Store storage type and field format array of fields */
  {
    /* prepare header */
    {
      uint32_t flags= 0;

      memset(buff, 0, format_section_header_size);
      /* length of section 2 bytes*/
      int2store(buff+0, format_section_len);
      /* flags of section 4 bytes*/
      int4store(buff+2, flags);
      /* 2 bytes left for future use */
    }
    /* write header */
    if (my_write(file, (const unsigned char*)buff, format_section_header_size, MYF_RW))
      goto err;
    buff[0]= 0;
    if (my_write(file, (const unsigned char*)buff, 1, MYF_RW))
      goto err;
    /* write column info, 1 byte per column */
    {
      List_iterator<Create_field> it(create_fields);
      Create_field *field;
      unsigned char column_format, write_byte;
      while ((field=it++))
      {
        column_format= (unsigned char)field->column_format();
        write_byte= (column_format << COLUMN_FORMAT_SHIFT);
        if (my_write(file, &write_byte, 1, MYF_RW))
          goto err;
      }
    }
  }
  lseek(file,filepos,SEEK_SET);
  if (my_write(file, forminfo, 288, MYF_RW) ||
      my_write(file, screen_buff, info_length, MYF_RW) ||
      pack_fields(file, create_fields, data_offset))
    goto err;

  free(screen_buff);
  free(keybuff);

  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      (my_sync(file, MYF(MY_WME)) ||
       my_sync_dir_by_file(file_name, MYF(MY_WME))))
      goto err2;

  if (my_close(file,MYF(MY_WME)))
    goto err3;

  {
    /*
      Restore all UCS2 intervals.
      HEX representation of them is not needed anymore.
    */
    List_iterator<Create_field> it(create_fields);
    Create_field *field;
    while ((field=it++))
    {
      if (field->save_interval)
      {
        field->interval= field->save_interval;
        field->save_interval= 0;
      }
    }
  }
  return(0);

err:
  free(screen_buff);
  free(keybuff);
err2:
  my_close(file,MYF(MY_WME));
err3:
  my_delete(file_name,MYF(0));
  return(1);
} /* mysql_create_frm */

static void fill_table_proto(drizzle::Table *table_proto,
                             const char *table_name,
                             List<Create_field> &create_fields,
                             HA_CREATE_INFO *create_info,
                             uint32_t keys,
                             KEY *key_info)
{
  Create_field *field_arg;
  List_iterator<Create_field> it(create_fields);
  drizzle::Table::StorageEngine *engine= table_proto->mutable_engine();
  drizzle::Table::TableOptions *table_options= table_proto->mutable_options();

  engine->set_name(create_info->db_type->name);

  table_proto->set_name(table_name);
  table_proto->set_type(drizzle::Table::STANDARD);

  while ((field_arg= it++))
  {
    drizzle::Table::Field *attribute;

    attribute= table_proto->add_field();
    attribute->set_name(field_arg->field_name);

    attribute->set_pack_flag(field_arg->pack_flag); /* TODO: MUST DIE */

    if(f_maybe_null(field_arg->pack_flag))
    {
      drizzle::Table::Field::FieldConstraints *constraints;

      constraints= attribute->mutable_constraints();
      constraints->set_is_nullable(true);
    }

    switch (field_arg->sql_type) {
    case DRIZZLE_TYPE_TINY:
      attribute->set_type(drizzle::Table::Field::TINYINT);
      break;
    case DRIZZLE_TYPE_LONG:
      attribute->set_type(drizzle::Table::Field::INTEGER);
      break;
    case DRIZZLE_TYPE_DOUBLE:
      attribute->set_type(drizzle::Table::Field::DOUBLE);
      break;
    case DRIZZLE_TYPE_NULL  :
      assert(1); /* Not a user definable type */
    case DRIZZLE_TYPE_TIMESTAMP:
      attribute->set_type(drizzle::Table::Field::TIMESTAMP);
      break;
    case DRIZZLE_TYPE_LONGLONG:
      attribute->set_type(drizzle::Table::Field::BIGINT);
      break;
    case DRIZZLE_TYPE_TIME:
      attribute->set_type(drizzle::Table::Field::TIME);
      break;
    case DRIZZLE_TYPE_DATETIME:
      attribute->set_type(drizzle::Table::Field::DATETIME);
      break;
    case DRIZZLE_TYPE_DATE:
      attribute->set_type(drizzle::Table::Field::DATE);
      break;
    case DRIZZLE_TYPE_VARCHAR:
      {
        drizzle::Table::Field::StringFieldOptions *string_field_options;

        string_field_options= attribute->mutable_string_options();
        attribute->set_type(drizzle::Table::Field::VARCHAR);
        string_field_options->set_length(field_arg->length
					 / field_arg->charset->mbmaxlen);
        string_field_options->set_collation_id(field_arg->charset->number);
        string_field_options->set_collation(field_arg->charset->name);

        break;
      }
    case DRIZZLE_TYPE_NEWDECIMAL:
      {
        drizzle::Table::Field::NumericFieldOptions *numeric_field_options;

        attribute->set_type(drizzle::Table::Field::DECIMAL);
        numeric_field_options= attribute->mutable_numeric_options();
        /* This is magic, I hate magic numbers -Brian */
        numeric_field_options->set_precision(field_arg->length + ( field_arg->decimals ? -2 : -1));
        numeric_field_options->set_scale(field_arg->decimals);
        break;
      }
    case DRIZZLE_TYPE_ENUM:
      {
        drizzle::Table::Field::SetFieldOptions *set_field_options;

        assert(field_arg->interval);

        attribute->set_type(drizzle::Table::Field::ENUM);
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
	attribute->set_type(drizzle::Table::Field::BLOB);

        drizzle::Table::Field::StringFieldOptions *string_field_options;

        string_field_options= attribute->mutable_string_options();
        string_field_options->set_collation_id(field_arg->charset->number);
        string_field_options->set_collation(field_arg->charset->name);
      }

      break;
    default:
      abort();
    }

    if(field_arg->vcol_info)
    {
      drizzle::Table::Field::VirtualFieldOptions *field_options;

      field_options= attribute->mutable_virtual_options();

      field_options->set_type(attribute->type());
      attribute->set_type(drizzle::Table::Field::VIRTUAL);

      string expr(field_arg->vcol_info->expr_str.str,
		  field_arg->vcol_info->expr_str.length);

      field_options->set_expression(expr);
      field_options->set_physically_stored(field_arg->is_stored);
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
      attribute->set_format(drizzle::Table::Field::DefaultFormat);
      break;
    case COLUMN_FORMAT_TYPE_FIXED:
      attribute->set_format(drizzle::Table::Field::FixedFormat);
      break;
    case COLUMN_FORMAT_TYPE_DYNAMIC:
      attribute->set_format(drizzle::Table::Field::DynamicFormat);
      break;
    default:
      abort();
    }

    if (field_arg->comment.length)
      attribute->set_comment(field_arg->comment.str);

    if(field_arg->unireg_check == Field::NEXT_NUMBER)
    {
      drizzle::Table::Field::NumericFieldOptions *field_options;
      field_options= attribute->mutable_numeric_options();
      field_options->set_is_autoincrement(true);
    }

    if(field_arg->unireg_check == Field::TIMESTAMP_DN_FIELD
       || field_arg->unireg_check == Field::TIMESTAMP_DNUN_FIELD)
    {
      drizzle::Table::Field::FieldOptions *field_options;
      field_options= attribute->mutable_options();
      field_options->set_default_value("NOW()");
    }

    if(field_arg->unireg_check == Field::TIMESTAMP_UN_FIELD
       || field_arg->unireg_check == Field::TIMESTAMP_DNUN_FIELD)
    {
      drizzle::Table::Field::FieldOptions *field_options;
      field_options= attribute->mutable_options();
      field_options->set_update_value("NOW()");
    }

    if(field_arg->def)
    {
      drizzle::Table::Field::FieldOptions *field_options;
      field_options= attribute->mutable_options();

      if(field_arg->def->is_null())
      {
	field_options->set_default_null(true);
      }
      else
      {
	String d;
	String *default_value= field_arg->def->val_str(&d);

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
      drizzle::Table::Field::FieldOptions *field_options;
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
    table_options->set_row_type(drizzle::Table::TableOptions::ROW_TYPE_DEFAULT);
    break;
  case ROW_TYPE_FIXED:
    table_options->set_row_type(drizzle::Table::TableOptions::ROW_TYPE_FIXED);
    break;
  case ROW_TYPE_DYNAMIC:
    table_options->set_row_type(drizzle::Table::TableOptions::ROW_TYPE_DYNAMIC);
    break;
  case ROW_TYPE_COMPRESSED:
    table_options->set_row_type(drizzle::Table::TableOptions::ROW_TYPE_COMPRESSED);
    break;
  case ROW_TYPE_REDUNDANT:
    table_options->set_row_type(drizzle::Table::TableOptions::ROW_TYPE_REDUNDANT);
    break;
  case ROW_TYPE_COMPACT:
    table_options->set_row_type(drizzle::Table::TableOptions::ROW_TYPE_COMPACT);
    break;
  case ROW_TYPE_PAGE:
    table_options->set_row_type(drizzle::Table::TableOptions::ROW_TYPE_PAGE);
    break;
  default:
    abort();
  }

  table_options->set_pack_record(create_info->table_options
				 & HA_OPTION_PACK_RECORD);

  if (create_info->comment.length)
    table_options->set_comment(create_info->comment.str);

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
    drizzle::Table::Index *idx;

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
      idx->set_type(drizzle::Table::Index::HASH);
      break;

    case HA_KEY_ALG_BTREE:
      idx->set_type(drizzle::Table::Index::BTREE);
      break;

    case HA_KEY_ALG_RTREE:
      idx->set_type(drizzle::Table::Index::RTREE);
    case HA_KEY_ALG_FULLTEXT:
      idx->set_type(drizzle::Table::Index::FULLTEXT);
    case HA_KEY_ALG_UNDEF:
      idx->set_type(drizzle::Table::Index::UNKNOWN_INDEX);
      break;

    default:
      abort(); /* Somebody's brain broke. haven't added index type to proto */
    }

    if (key_info[i].flags & HA_NOSAME)
      idx->set_is_unique(true);
    else
      idx->set_is_unique(false);

    drizzle::Table::Index::IndexOptions *index_options= idx->mutable_options();

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
      idx->set_comment(key_info[i].comment.str);

    if(key_info[i].flags & ~(HA_NOSAME | HA_PACK_KEY | HA_USES_BLOCK_SIZE | HA_BINARY_PACK_KEY | HA_VAR_LENGTH_PART | HA_NULL_PART_KEY | HA_KEY_HAS_PART_KEY_SEG | HA_GENERATED_KEY | HA_USES_COMMENT))
      abort(); // Invalid (unknown) index flag.

    for(unsigned int j=0; j< key_info[i].key_parts; j++)
    {
      drizzle::Table::Index::IndexPart *idxpart;

      idxpart= idx->add_index_part();

      idxpart->set_fieldnr(key_info[i].key_part[j].fieldnr);

      idxpart->set_compare_length(key_info[i].key_part[j].length);

      idxpart->set_key_type(key_info[i].key_part[j].key_type);

    }
  }
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

bool create_table_proto_file(const char *file_name,
                             const char *table_name,
                             HA_CREATE_INFO *create_info,
                             List<Create_field> &create_fields,
                             uint32_t keys,
                             KEY *key_info)
{
  drizzle::Table table_proto;
  string new_path(file_name);
  string file_ext = ".dfe";

  fill_table_proto(&table_proto, table_name, create_fields, create_info,
                   keys, key_info);

  new_path.replace(new_path.find(".frm"), file_ext.length(), file_ext );

  fstream output(new_path.c_str(), ios::out | ios::trunc | ios::binary);
  if (!table_proto.SerializeToOstream(&output))
  {
    fprintf(stderr, "Failed to write schema.\n");

    return true;
  }

  return false;
}

/*
  Create a frm (table definition) file and the tables

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
                     HA_CREATE_INFO *create_info,
                     List<Create_field> &create_fields,
                     uint32_t keys, KEY *key_info, handler *file,
                     bool is_like)
{
  char frm_name[FN_REFLEN];

  /* Proto will blow up unless we give a name */
  assert(table_name);

  /* For is_like we return once the file has been created */
  if (is_like)
  {
    if (mysql_create_frm(session, path, db, table_name, create_info,
                         create_fields, keys, key_info, file))
      return 1;

    if (create_table_proto_file(path, table_name, create_info,
                                create_fields, keys, key_info))
      return 1;

    return 0;
  }
  /* Here we need to build the full frm from the path */
  else
  {
    sprintf(frm_name,"%s%s", path, reg_ext);

    if (mysql_create_frm(session, frm_name, db, table_name, create_info,
                         create_fields, keys, key_info, file))
      return 1;

    if (create_table_proto_file(frm_name, table_name, create_info,
                                create_fields, keys, key_info))
      return 1;
  }

  // Make sure mysql_create_frm din't remove extension
  assert(*fn_rext(frm_name));
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
  my_delete(frm_name, MYF(0));

  delete_table_proto_file(path);

  return 1;
} /* rea_create_table */


	/* Pack screens to a screen for save in a form-file */

static unsigned char *pack_screens(List<Create_field> &create_fields,
                           uint32_t *info_length, uint32_t *screens,
                           bool small_file)
{
  register uint32_t i;
  uint32_t row,start_row,end_row,fields_on_screen;
  uint32_t length,cols, cols_half_len;
  unsigned char *info,*pos,*start_screen;
  uint32_t fields=create_fields.elements;
  List_iterator<Create_field> it(create_fields);

  start_row=4; end_row=22; cols=80; fields_on_screen=end_row+1-start_row;
  cols_half_len= (uint32_t)(cols >> 1);

  *screens=(fields-1)/fields_on_screen+1;
  length= (*screens) * (SC_INFO_LENGTH+cols_half_len+4);

  Create_field *field;
  while ((field=it++))
    length+=(uint32_t) strlen(field->field_name)+1+TE_INFO_LENGTH+cols/2;

  if (!(info=(unsigned char*) malloc(length)))
    return(0);

  start_screen=0;
  row=end_row;
  pos=info;
  it.rewind();
  for (i=0 ; i < fields ; i++)
  {
    Create_field *cfield=it++;
    if (row++ == end_row)
    {
      if (i)
      {
        length=(uint32_t) (pos-start_screen);
        int2store(start_screen,length);
        start_screen[2]=(unsigned char) (fields_on_screen+1);
        start_screen[3]=(unsigned char) (fields_on_screen);
      }
      row= start_row;
      start_screen=pos;
      pos+=4;
      pos[0]= (unsigned char) start_row-2;	/* Header string */
      pos[1]= (unsigned char) (cols >> 2);
      pos[2]= (unsigned char) (cols >> 1) +1;
      memset((char*)pos+3,' ',cols_half_len);
      pos[cols_half_len+3]= '\0';
      pos+= cols_half_len+4;
    }
    length= (uint32_t) strlen(cfield->field_name);
    if (length > cols-3)
      length=cols-3;

    if (!small_file)
    {
      pos[0]=(unsigned char) row;
      pos[1]=0;
      pos[2]=(unsigned char) (length+1);
      strncpy((char*)pos+3,cfield->field_name, length);
      pos[length + 3]= '\0';
      pos+= length + 3 + 1;
    }
  }
  length=(uint32_t) (pos-start_screen);
  int2store(start_screen,length);
  start_screen[2]=(unsigned char) (row-start_row+2);
  start_screen[3]=(unsigned char) (row-start_row+1);

  *info_length=(uint32_t) (pos-info);
  return(info);
} /* pack_screens */


	/* Pack keyinfo and keynames to keybuff for save in form-file. */

static uint32_t pack_keys(unsigned char *keybuff, uint32_t key_count, KEY *keyinfo,
                      ulong data_offset)
{
  uint32_t key_parts,length;
  unsigned char *pos, *keyname_pos;
  KEY *key,*end;
  KEY_PART_INFO *key_part,*key_part_end;


  pos=keybuff+6;
  key_parts=0;
  for (key=keyinfo,end=keyinfo+key_count ; key != end ; key++)
  {
    int2store(pos, (key->flags ^ HA_NOSAME));
    int2store(pos+2,key->key_length);
    pos[4]= (unsigned char) key->key_parts;
    pos[5]= (unsigned char) key->algorithm;
    int2store(pos+6, key->block_size);
    pos+=8;
    key_parts+=key->key_parts;
    for (key_part=key->key_part,key_part_end=key_part+key->key_parts ;
	 key_part != key_part_end ;
	 key_part++)

    {
      uint32_t offset;
      int2store(pos,key_part->fieldnr+1+FIELD_NAME_USED);
      offset= (uint32_t) (key_part->offset+data_offset+1);
      int2store(pos+2, offset);
      pos[4]=0;					// Sort order
      int2store(pos+5,key_part->key_type);
      int2store(pos+7,key_part->length);
      pos+=9;
    }
  }
	/* Save keynames */
  keyname_pos=pos;
  *pos++=(unsigned char) NAMES_SEP_CHAR;
  for (key=keyinfo ; key != end ; key++)
  {
    unsigned char *tmp=(unsigned char*) strcpy((char*) pos,key->name);
    tmp+= strlen(key->name);
    *tmp++= (unsigned char) NAMES_SEP_CHAR;
    *tmp=0;
    pos=tmp;
  }
  *(pos++)=0;

  for (key=keyinfo,end=keyinfo+key_count ; key != end ; key++)
  {
    if (key->flags & HA_USES_COMMENT)
    {
      int2store(pos, key->comment.length);
      unsigned char *tmp= (unsigned char*)strncpy((char*) pos+2,key->comment.str,key->comment.length);
      tmp+= key->comment.length;
      pos= tmp;
    }
  }

  if (key_count > 127 || key_parts > 127)
  {
    keybuff[0]= (key_count & 0x7f) | 0x80;
    keybuff[1]= key_count >> 7;
    int2store(keybuff+2,key_parts);
  }
  else
  {
    keybuff[0]=(unsigned char) key_count;
    keybuff[1]=(unsigned char) key_parts;
    keybuff[2]= keybuff[3]= 0;
  }
  length=(uint32_t) (pos-keyname_pos);
  int2store(keybuff+4,length);
  return((uint32_t) (pos-keybuff));
} /* pack_keys */


/* Make formheader */

static bool pack_header(unsigned char *forminfo,
                        List<Create_field> &create_fields,
                        uint32_t info_length, uint32_t screens,
                        ulong data_offset, handler *file)
{
  uint32_t length,int_count,int_length, int_parts;
  uint32_t time_stamp_pos,null_fields;
  ulong reclength, totlength, n_length, com_length, vcol_info_length;


  if (create_fields.elements > MAX_FIELDS)
  {
    my_message(ER_TOO_MANY_FIELDS, ER(ER_TOO_MANY_FIELDS), MYF(0));
    return(1);
  }

  totlength= 0L;
  reclength= data_offset;
  int_count=int_parts=int_length=time_stamp_pos=null_fields=
    com_length=vcol_info_length=0;
  n_length=2L;

	/* Check fields */

  List_iterator<Create_field> it(create_fields);
  Create_field *field;
  while ((field=it++))
  {
    uint32_t tmp_len= system_charset_info->cset->charpos(system_charset_info,
                                                     field->comment.str,
                                                     field->comment.str +
                                                     field->comment.length,
                                                     COLUMN_COMMENT_MAXLEN);

    if (tmp_len < field->comment.length)
    {
      my_error(ER_WRONG_STRING_LENGTH, MYF(0),
               field->comment.str,"COLUMN COMMENT",
               (uint32_t) COLUMN_COMMENT_MAXLEN);
      return(1);
    }
    if (field->vcol_info)
    {
      tmp_len= system_charset_info->cset->charpos(system_charset_info,
                                                  field->vcol_info->expr_str.str,
                                                  field->vcol_info->expr_str.str +
                                                  field->vcol_info->expr_str.length,
                                                  VIRTUAL_COLUMN_EXPRESSION_MAXLEN);

      if (tmp_len < field->vcol_info->expr_str.length)
      {
        my_error(ER_WRONG_STRING_LENGTH, MYF(0),
                 field->vcol_info->expr_str.str,"VIRTUAL COLUMN EXPRESSION",
                 (uint32_t) VIRTUAL_COLUMN_EXPRESSION_MAXLEN);
        return(1);
      }
      /*
        Sum up the length of the expression string and mandatory header bytes
        to the total length.
      */
      vcol_info_length+= field->vcol_info->expr_str.length+(uint32_t)FRM_VCOL_HEADER_SIZE;
    }

    totlength+= field->length;
    com_length+= field->comment.length;
    /*
      We mark first TIMESTAMP field with NOW() in DEFAULT or ON UPDATE
      as auto-update field.
    */
    if (field->sql_type == DRIZZLE_TYPE_TIMESTAMP &&
        MTYP_TYPENR(field->unireg_check) != Field::NONE &&
	!time_stamp_pos)
      time_stamp_pos= (uint32_t) field->offset+ (uint32_t) data_offset + 1;
    length=field->pack_length;
    if ((uint32_t) field->offset+ (uint32_t) data_offset+ length > reclength)
      reclength=(uint32_t) (field->offset+ data_offset + length);
    n_length+= (ulong) strlen(field->field_name)+1;
    field->interval_id=0;
    field->save_interval= 0;
    if (field->interval)
    {
      uint32_t old_int_count=int_count;

      if (field->charset->mbminlen > 1)
      {
        /*
          Escape UCS2 intervals using HEX notation to avoid
          problems with delimiters between enum elements.
          As the original representation is still needed in
          the function make_empty_rec to create a record of
          filled with default values it is saved in save_interval
          The HEX representation is created from this copy.
        */
        field->save_interval= field->interval;
        field->interval= (TYPELIB*) sql_alloc(sizeof(TYPELIB));
        *field->interval= *field->save_interval;
        field->interval->type_names=
          (const char **) sql_alloc(sizeof(char*) *
				    (field->interval->count+1));
        field->interval->type_names[field->interval->count]= 0;
        field->interval->type_lengths=
          (uint32_t *) sql_alloc(sizeof(uint32_t) * field->interval->count);

        for (uint32_t pos= 0; pos < field->interval->count; pos++)
        {
          char *dst;
          const char *src= field->save_interval->type_names[pos];
          uint32_t hex_length;
          length= field->save_interval->type_lengths[pos];
          hex_length= length * 2;
          field->interval->type_lengths[pos]= hex_length;
          field->interval->type_names[pos]= dst= (char*) sql_alloc(hex_length +
                                                                   1);
          drizzleclient_drizzleclient_octet2hex(dst, src, length);
        }
      }

      field->interval_id=get_interval_id(&int_count,create_fields,field);
      if (old_int_count != int_count)
      {
	for (const char **pos=field->interval->type_names ; *pos ; pos++)
	  int_length+=(uint32_t) strlen(*pos)+1;	// field + suffix prefix
	int_parts+=field->interval->count+1;
      }
    }
    if (f_maybe_null(field->pack_flag))
      null_fields++;
  }
  int_length+=int_count*2;			// 255 prefix + 0 suffix

	/* Save values in forminfo */

  if (reclength > (ulong) file->max_record_length())
  {
    my_error(ER_TOO_BIG_ROWSIZE, MYF(0), (uint32_t) file->max_record_length());
    return(1);
  }

  if (info_length+(ulong) create_fields.elements*FCOMP+288+
      n_length+int_length+com_length+vcol_info_length > 65535L ||
      int_count > 255)
  {
    my_message(ER_TOO_MANY_FIELDS, ER(ER_TOO_MANY_FIELDS), MYF(0));
    return(1);
  }

  memset(forminfo, 0, 288);
  length=(info_length+create_fields.elements*FCOMP+288+n_length+int_length+
	  com_length+vcol_info_length);
  int2store(forminfo,length);
  forminfo[256] = (uint8_t) screens;
  int2store(forminfo+258,create_fields.elements);
  int2store(forminfo+260,info_length);
  int2store(forminfo+262,totlength);
  int2store(forminfo+264,0); // no_empty
  int2store(forminfo+266,reclength);
  int2store(forminfo+268,n_length);
  int2store(forminfo+270,int_count);
  int2store(forminfo+272,int_parts);
  int2store(forminfo+274,int_length);
  int2store(forminfo+276,time_stamp_pos);
  int2store(forminfo+278,80);			/* Columns needed */
  int2store(forminfo+280,22);			/* Rows needed */
  int2store(forminfo+282,null_fields);
  int2store(forminfo+284,com_length);
  int2store(forminfo+286,vcol_info_length);
  /* forminfo+288 is free to use for additional information */
  return(0);
} /* pack_header */


	/* get each unique interval each own id */

static uint32_t get_interval_id(uint32_t *int_count,List<Create_field> &create_fields,
			    Create_field *last_field)
{
  List_iterator<Create_field> it(create_fields);
  Create_field *field;
  TYPELIB *interval=last_field->interval;

  while ((field=it++) != last_field)
  {
    if (field->interval_id && field->interval->count == interval->count)
    {
      const char **a,**b;
      for (a=field->interval->type_names, b=interval->type_names ;
	   *a && !strcmp(*a,*b);
	   a++,b++) ;

      if (! *a)
      {
	return field->interval_id;		// Re-use last interval
      }
    }
  }
  return ++*int_count;				// New unique interval
}


	/* Save fields, fieldnames and intervals */

static bool pack_fields(File file, List<Create_field> &create_fields,
                        ulong data_offset)
{
  register uint32_t field_element;
  uint32_t int_count, comment_length=0, vcol_info_length=0;
  unsigned char buff[MAX_FIELD_WIDTH];
  Create_field *field;


	/* Write field info */

  List_iterator<Create_field> it(create_fields);

  int_count=0;
  while ((field=it++))
  {
    uint32_t recpos;
    uint32_t cur_vcol_expr_len= 0;
    buff[0]= 0; //(unsigned char) field->row;
    buff[1]= 0; //(unsigned char) field->col;
    buff[2]= 0; //(unsigned char) field->sc_length;
    int2store(buff+3, field->length);
    /* The +1 is here becasue the col offset in .frm file have offset 1 */
    recpos= field->offset+1 + (uint32_t) data_offset;
    int3store(buff+5,recpos);
    int2store(buff+8,field->pack_flag);
    int2store(buff+10,field->unireg_check);
    buff[12]= (unsigned char) field->interval_id;
    buff[13]= (unsigned char) field->sql_type;
    if (field->charset)
      buff[14]= (unsigned char) field->charset->number;
    else
      buff[14]= 0;				// Numerical
    if (field->vcol_info)
    {
      /*
        Use the interval_id place in the .frm file to store the length of
        virtual field's data.
      */
      buff[12]= cur_vcol_expr_len= field->vcol_info->expr_str.length +
                (uint32_t)FRM_VCOL_HEADER_SIZE;
      vcol_info_length+= cur_vcol_expr_len+(uint32_t)FRM_VCOL_HEADER_SIZE;
      buff[13]= (unsigned char) DRIZZLE_TYPE_VIRTUAL;
    }
    int2store(buff+15, field->comment.length);
    comment_length+= field->comment.length;
    set_if_bigger(int_count,field->interval_id);
    if (my_write(file, buff, FCOMP, MYF_RW))
      return(1);
  }

	/* Write fieldnames */
  buff[0]=(unsigned char) NAMES_SEP_CHAR;
  if (my_write(file, buff, 1, MYF_RW))
    return(1);
  field_element=0;
  it.rewind();
  while ((field=it++))
  {
    char *pos= strcpy((char*) buff,field->field_name);
    pos+= strlen(field->field_name);
    *pos++=NAMES_SEP_CHAR;
    if (field_element == create_fields.elements-1)
      *pos++=0;
    if (my_write(file, buff, (size_t) (pos-(char*) buff),MYF_RW))
      return(1);
    field_element++;
  }

	/* Write intervals */
  if (int_count)
  {
    String tmp((char*) buff,sizeof(buff), &my_charset_bin);
    tmp.length(0);
    it.rewind();
    int_count=0;
    while ((field=it++))
    {
      if (field->interval_id > int_count)
      {
        unsigned char  sep= 0;
        unsigned char  occ[256];
        uint32_t       length_index;
        unsigned char *val= NULL;

        memset(occ, 0, sizeof(occ));

        for (length_index=0;
             (val= (unsigned char*) field->interval->type_names[length_index]);
              length_index++)
          for (uint32_t j = 0;
               j < field->interval->type_lengths[length_index];
               j++)
            occ[(unsigned int) (val[j])]= 1;

        if (!occ[(unsigned char)NAMES_SEP_CHAR])
          sep= (unsigned char) NAMES_SEP_CHAR;
        else if (!occ[(unsigned int)','])
          sep= ',';
        else
        {
          for (uint32_t x=1; x<256; x++)
          {
            if(!occ[x])
            {
              sep= x;
              break;
            }
          }

          if(!sep)    /* disaster, enum uses all characters, none left as separator */
          {
            my_message(ER_WRONG_FIELD_TERMINATORS,ER(ER_WRONG_FIELD_TERMINATORS),
                       MYF(0));
            return(1);
          }
        }

        int_count= field->interval_id;
        tmp.append(sep);
        for (const char **pos=field->interval->type_names ; *pos ; pos++)
        {
          tmp.append(*pos);
          tmp.append(sep);
        }
        tmp.append('\0');                      // End of intervall
      }
    }
    if (my_write(file,(unsigned char*) tmp.ptr(),tmp.length(),MYF_RW))
      return(1);
  }
  if (comment_length)
  {
    it.rewind();
    int_count=0;
    while ((field=it++))
    {
      if (field->comment.length)
	if (my_write(file, (unsigned char*) field->comment.str, field->comment.length,
		     MYF_RW))
	  return(1);
    }
  }
  if (vcol_info_length)
  {
    it.rewind();
    int_count=0;
    while ((field=it++))
    {
      /*
        Pack each virtual field as follows:
        byte 1      = 1 (always 1 to allow for future extensions)
        byte 2      = sql_type
        byte 3      = flags (as of now, 0 - no flags, 1 - field is physically stored)
        byte 4-...  = virtual column expression (text data)
      */
      if (field->vcol_info && field->vcol_info->expr_str.length)
      {
        buff[0]= (unsigned char)1;
        buff[1]= (unsigned char) field->sql_type;
        buff[2]= (unsigned char) field->is_stored;
        if (my_write(file, buff, 3, MYF_RW))
          return(1);
        if (my_write(file,
                     (unsigned char*) field->vcol_info->expr_str.str,
                     field->vcol_info->expr_str.length,
                     MYF_RW))
          return(1);
      }
    }
  }
  return(0);
}


/* save an empty record on start of formfile */

static bool make_empty_rec(Session *session, File file,
                           enum legacy_db_type ,
                           uint32_t table_options,
                           List<Create_field> &create_fields,
                           uint32_t reclength,
                           ulong data_offset,
                           handler *handler)
{
  int error= 0;
  Field::utype type;
  uint32_t null_count;
  unsigned char *buff,*null_pos;
  Table table;
  TABLE_SHARE share;
  Create_field *field;
  enum_check_fields old_count_cuted_fields= session->count_cuted_fields;


  /* We need a table to generate columns for default values */
  memset(&table, 0, sizeof(table));
  memset(&share, 0, sizeof(share));
  table.s= &share;

  if (!(buff=(unsigned char*) malloc((size_t) reclength)))
  {
    return(1);
  }
  memset(buff, 0, (size_t) reclength);

  table.in_use= session;
  table.s->db_low_byte_first= handler->low_byte_first();
  table.s->blob_ptr_size= portable_sizeof_char_ptr;

  null_count=0;
  if (!(table_options & HA_OPTION_PACK_RECORD))
  {
    null_count++;			// Need one bit for delete mark
    *buff|= 1;
  }
  null_pos= buff;

  List_iterator<Create_field> it(create_fields);
  session->count_cuted_fields= CHECK_FIELD_WARN;    // To find wrong default values
  while ((field=it++))
  {
    /*
      regfield don't have to be deleted as it's allocated with sql_alloc()
    */
    Field *regfield= make_field(&share, NULL,
                                buff+field->offset + data_offset,
                                field->length,
                                null_pos + null_count / 8,
                                null_count & 7,
                                field->pack_flag,
                                field->sql_type,
                                field->charset,
                                field->unireg_check,
                                field->save_interval ? field->save_interval :
                                field->interval,
                                field->field_name);
    if (!regfield)
    {
      error= 1;
      goto err;                                 // End of memory
    }

    /* save_in_field() will access regfield->table->in_use */
    regfield->init(&table);

    if (!(field->flags & NOT_NULL_FLAG))
    {
      *regfield->null_ptr|= regfield->null_bit;
      null_count++;
    }

    type= (Field::utype) MTYP_TYPENR(field->unireg_check);

    if (field->def)
    {
      int res= field->def->save_in_field(regfield, 1);
      /* If not ok or warning of level 'note' */
      if (res != 0 && res != 3)
      {
        my_error(ER_INVALID_DEFAULT, MYF(0), regfield->field_name);
        error= 1;
        delete regfield; //To avoid memory leak
        goto err;
      }
    }
    else if (regfield->real_type() == DRIZZLE_TYPE_ENUM &&
	     (field->flags & NOT_NULL_FLAG))
    {
      regfield->set_notnull();
      regfield->store((int64_t) 1, true);
    }
    else
      regfield->reset();
  }
  assert(data_offset == ((null_count + 7) / 8));

  /*
    We need to set the unused bits to 1. If the number of bits is a multiple
    of 8 there are no unused bits.
  */
  if (null_count & 7)
    *(null_pos + null_count / 8)|= ~(((unsigned char) 1 << (null_count & 7)) - 1);

  error= my_write(file, buff, (size_t) reclength,MYF_RW) != 0;

err:
  free(buff);
  session->count_cuted_fields= old_count_cuted_fields;
  return(error);
} /* make_empty_rec */
