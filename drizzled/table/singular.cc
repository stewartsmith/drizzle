/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <drizzled/session.h>
#include <plugin/myisam/myisam.h>
#include <drizzled/plugin/transactional_storage_engine.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/table.h>

namespace drizzled {
namespace table {

Singular::Singular(Session *session, std::list<CreateField>& field_list) :
  _share(message::Table::INTERNAL),
  _has_variable_width(false)
{
  uint32_t field_count= field_list.size();
  uint32_t blob_count= 0;
  uint32_t record_length= 0;
  uint32_t null_count= 0;                 /* number of columns which may be null */
  uint32_t null_pack_length;              /* NULL representation array length */

  getMutableShare()->setFields(field_count + 1);
  setFields(getMutableShare()->getFields(true));
  Field** field_arg= getMutableShare()->getFields(true);
  getMutableShare()->blob_field.resize(field_count+1);
  getMutableShare()->setFieldSize(field_count);
  getMutableShare()->blob_ptr_size= portable_sizeof_char_ptr;
  setup_tmp_table_column_bitmaps();

  in_use= session;           /* field_arg->reset() may access in_use */

  /* Create all fields and calculate the total length of record */
  message::Table::Field null_field;
	BOOST_FOREACH(CreateField& it, field_list)
  {
    *field_arg= getMutableShare()->make_field(null_field,
                                              NULL,
                                              it.length,
                                              (it.flags & NOT_NULL_FLAG) ? false : true,
                                              (unsigned char *) ((it.flags & NOT_NULL_FLAG) ? 0 : ""),
                                              (it.flags & NOT_NULL_FLAG) ? 0 : 1,
                                              it.decimals,
                                              it.sql_type,
                                              it.charset,
                                              it.unireg_check,
                                              it.interval,
                                              it.field_name,
                                              it.flags & UNSIGNED_FLAG ? true : false);
    (*field_arg)->init(this);
    record_length+= (*field_arg)->pack_length();
    if (! ((*field_arg)->flags & NOT_NULL_FLAG))
      null_count++;

    if ((*field_arg)->flags & BLOB_FLAG)
      getMutableShare()->blob_field[blob_count++]= (uint32_t) (field_arg - getFields());

    field_arg++;
  }
  *field_arg= NULL;                             /* mark the end of the list */
  getMutableShare()->blob_field[blob_count]= 0;            /* mark the end of the list */
  getMutableShare()->blob_fields= blob_count;

  null_pack_length= (null_count + 7)/8;
  getMutableShare()->setRecordLength(record_length + null_pack_length);
  getMutableShare()->rec_buff_length= ALIGN_SIZE(getMutableShare()->getRecordLength() + 1);
  record[0]= session->mem.alloc(getMutableShare()->rec_buff_length);

  if (null_pack_length)
  {
    null_flags= getInsertRecord();
    getMutableShare()->null_fields= null_count;
    getMutableShare()->null_bytes= null_pack_length;
  }
  {
    /* Set up field pointers */
    unsigned char *null_pos= getInsertRecord();
    unsigned char *field_pos= null_pos + getMutableShare()->null_bytes;
    uint32_t null_bit= 1;

    for (field_arg= getFields(); *field_arg; ++field_arg)
    {
      Field *cur_field= *field_arg;
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
}

bool Singular::open_tmp_table()
{
  int error;
  
  identifier::Table identifier(getShare()->getSchemaName(), getShare()->getTableName(), getShare()->getPath());
  if ((error=cursor->ha_open(identifier,
                             O_RDWR,
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

bool Singular::create_myisam_tmp_table(KeyInfo *keyinfo,
                                                 MI_COLUMNDEF *start_recinfo,
                                                 MI_COLUMNDEF **recinfo,
                                                 uint64_t options)
{
  int error;
  MI_KEYDEF keydef;
  MI_UNIQUEDEF uniquedef;

  if (getShare()->sizeKeys())
  {						// Get keys for ni_create
    bool using_unique_constraint= false;
    HA_KEYSEG *seg= new (mem()) HA_KEYSEG[keyinfo->key_parts];

    memset(seg, 0, sizeof(*seg) * keyinfo->key_parts);
    if (keyinfo->key_length >= cursor->getEngine()->max_key_length() ||
        keyinfo->key_parts > cursor->getEngine()->max_key_parts() ||
        getShare()->uniques)
    {
      /* Can't create a key; Make a unique constraint instead of a key */
      getMutableShare()->keys=    0;
      getMutableShare()->uniques= 1;
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
      getMutableShare()->setRecordLength(getShare()->getRecordLength() + MI_UNIQUE_HASH_LENGTH);
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
        seg->bit_start= (uint8_t)(key_field->pack_length() - getShare()->blob_ptr_size);
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
        seg->null_pos= (uint32_t) (key_field->null_ptr - getInsertRecord());
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

  if ((options & (OPTION_BIG_TABLES | SELECT_SMALL_RESULT)) ==
      OPTION_BIG_TABLES)
    create_info.data_file_length= ~(uint64_t) 0;

  if ((error= mi_create(getShare()->getTableName(), getShare()->sizeKeys(), &keydef,
                        (uint32_t) (*recinfo-start_recinfo),
                        start_recinfo,
                        getShare()->uniques, &uniquedef,
                        &create_info,
                        HA_CREATE_TMP_TABLE)))
  {
    print_error(error, MYF(0));
    db_stat= 0;

    return true;
  }
  in_use->status_var.created_tmp_disk_tables++;
  getMutableShare()->db_record_offset= 1;
  return false;
}

/*
  Set up column usage bitmaps for a temporary table

  IMPLEMENTATION
    For temporary tables, we need one bitmap with all columns set and
    a tmp_set bitmap to be used by things like filesort.
*/

void Singular::setup_tmp_table_column_bitmaps()
{
  uint32_t field_count= getShare()->sizeFields();

  def_read_set.resize(field_count);
  def_write_set.resize(field_count);
  tmp_set.resize(field_count);
  getMutableShare()->all_set.resize(field_count);
  getMutableShare()->all_set.set();
  def_write_set.set();
  def_read_set.set();
  default_column_bitmaps();
}

Singular::~Singular()
{
  const char* save_proc_info= in_use->get_proc_info();
  in_use->set_proc_info("removing tmp table");

  // Release latches since this can take a long time
  plugin::TransactionalStorageEngine::releaseTemporaryLatches(in_use);

  if (cursor)
  {
    if (db_stat)
    {
      cursor->closeMarkForDelete(getShare()->getTableName());
    }

    identifier::Table identifier(getShare()->getSchemaName(), getShare()->getTableName(), getShare()->getTableName());
    drizzled::error_t ignored;
    plugin::StorageEngine::dropTable(*in_use,
                                     *getShare()->getEngine(),
                                     identifier,
                                     ignored);

    delete cursor;
  }

  /* free blobs */
  for (Field **ptr= getFields(); *ptr; ptr++)
  {
    (*ptr)->free();
  }
  free_io_cache();

  mem().free_root(MYF(0));
  in_use->set_proc_info(save_proc_info);
}


} /* namespace table */
} /* namespace drizzled */
