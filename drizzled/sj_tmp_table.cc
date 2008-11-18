/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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
#include <drizzled/sj_tmp_table.h>
#include <drizzled/session.h>

/*
  Create a temporary table to weed out duplicate rowid combinations

  SYNOPSIS

  create_duplicate_weedout_tmp_table()
  session
  uniq_tuple_length_arg
  SJ_TMP_TABLE

  DESCRIPTION
  Create a temporary table to weed out duplicate rowid combinations. The
  table has a single column that is a concatenation of all rowids in the
  combination.

  Depending on the needed length, there are two cases:

  1. When the length of the column < max_key_length:

  CREATE TABLE tmp (col VARBINARY(n) NOT NULL, UNIQUE KEY(col));

  2. Otherwise (not a valid SQL syntax but internally supported):

  CREATE TABLE tmp (col VARBINARY NOT NULL, UNIQUE CONSTRAINT(col));

  The code in this function was produced by extraction of relevant parts
  from create_tmp_table().

  RETURN
  created table
  NULL on error
*/

Table *create_duplicate_weedout_tmp_table(Session *session,
                                          uint32_t uniq_tuple_length_arg,
                                          SJ_TMP_TABLE *sjtbl)
{
  MEM_ROOT *mem_root_save, own_root;
  Table *table;
  TABLE_SHARE *share;
  uint32_t  temp_pool_slot=MY_BIT_NONE;
  char  *tmpname,path[FN_REFLEN];
  Field **reg_field;
  KEY_PART_INFO *key_part_info;
  KEY *keyinfo;
  unsigned char *group_buff;
  unsigned char *bitmaps;
  uint32_t *blob_field;
  MI_COLUMNDEF *recinfo, *start_recinfo;
  bool using_unique_constraint=false;
  Field *field, *key_field;
  uint32_t blob_count, null_pack_length, null_count;
  unsigned char *null_flags;
  unsigned char *pos;
  
  /*
    STEP 1: Get temporary table name
  */
  statistic_increment(session->status_var.created_tmp_tables, &LOCK_status);
  if (use_temp_pool && !(test_flags & TEST_KEEP_TMP_TABLES))
    temp_pool_slot = bitmap_lock_set_next(&temp_pool);

  if (temp_pool_slot != MY_BIT_NONE) // we got a slot
    sprintf(path, "%s_%lx_%i", TMP_FILE_PREFIX,
	    current_pid, temp_pool_slot);
  else
  {
    /* if we run out of slots or we are not using tempool */
    sprintf(path,"%s%lx_%"PRIx64"_%x", TMP_FILE_PREFIX, current_pid,
            session->thread_id, session->tmp_table++);
  }
  fn_format(path, path, drizzle_tmpdir, "", MY_REPLACE_EXT|MY_UNPACK_FILENAME);

  /* STEP 2: Figure if we'll be using a key or blob+constraint */
  if (uniq_tuple_length_arg >= CONVERT_IF_BIGGER_TO_BLOB)
    using_unique_constraint= true;

  /* STEP 3: Allocate memory for temptable description */
  init_sql_alloc(&own_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  if (!multi_alloc_root(&own_root,
                        &table, sizeof(*table),
                        &share, sizeof(*share),
                        &reg_field, sizeof(Field*) * (1+1),
                        &blob_field, sizeof(uint32_t)*2,
                        &keyinfo, sizeof(*keyinfo),
                        &key_part_info, sizeof(*key_part_info) * 2,
                        &start_recinfo,
                        sizeof(*recinfo)*(1*2+4),
                        &tmpname, (uint32_t) strlen(path)+1,
                        &group_buff, (!using_unique_constraint ?
                                      uniq_tuple_length_arg : 0),
                        &bitmaps, bitmap_buffer_size(1)*2,
                        NULL))
  {
    if (temp_pool_slot != MY_BIT_NONE)
      bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
    return(NULL);
  }
  my_stpcpy(tmpname,path);

  /* STEP 4: Create Table description */
  memset(table, 0, sizeof(*table));
  memset(reg_field, 0, sizeof(Field*)*2);

  table->mem_root= own_root;
  mem_root_save= session->mem_root;
  session->mem_root= &table->mem_root;

  table->field=reg_field;
  table->alias= "weedout-tmp";
  table->reginfo.lock_type=TL_WRITE;  /* Will be updated */
  table->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  table->map=1;
  table->temp_pool_slot = temp_pool_slot;
  table->copy_blobs= 1;
  table->in_use= session;
  table->quick_keys.init();
  table->covering_keys.init();
  table->keys_in_use_for_query.init();

  table->s= share;
  init_tmp_table_share(session, share, "", 0, tmpname, tmpname);
  share->blob_field= blob_field;
  share->blob_ptr_size= portable_sizeof_char_ptr;
  share->db_low_byte_first=1;                // True for HEAP and MyISAM
  share->table_charset= NULL;
  share->primary_key= MAX_KEY;               // Indicate no primary key
  share->keys_for_keyread.init();
  share->keys_in_use.init();

  blob_count= 0;

  /* Create the field */
  {
    /*
      For the sake of uniformity, always use Field_varstring.
    */
    field= new Field_varstring(uniq_tuple_length_arg, false, "rowids", share,
                               &my_charset_bin);
    if (!field)
      return(0);
    field->table= table;
    field->key_start.init(0);
    field->part_of_key.init(0);
    field->part_of_sortkey.init(0);
    field->unireg_check= Field::NONE;
    field->flags= (NOT_NULL_FLAG | BINARY_FLAG | NO_DEFAULT_VALUE_FLAG);
    field->reset_fields();
    field->init(table);
    field->orig_table= NULL;

    field->field_index= 0;

    *(reg_field++)= field;
    *blob_field= 0;
    *reg_field= 0;

    share->fields= 1;
    share->blob_fields= 0;
  }

  uint32_t reclength= field->pack_length();
  if (using_unique_constraint)
  {
    share->db_plugin= ha_lock_engine(0, myisam_hton);
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
    assert(uniq_tuple_length_arg <= table->file->max_key_length());
  }
  else
  {
    share->db_plugin= ha_lock_engine(0, heap_hton);
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
  }
  if (!table->file)
    goto err;

  null_count=1;

  null_pack_length= 1;
  reclength += null_pack_length;

  share->reclength= reclength;
  {
    uint32_t alloc_length=ALIGN_SIZE(share->reclength + MI_UNIQUE_HASH_LENGTH+1);
    share->rec_buff_length= alloc_length;
    if (!(table->record[0]= (unsigned char*)
          alloc_root(&table->mem_root, alloc_length*3)))
      goto err;
    table->record[1]= table->record[0]+alloc_length;
    share->default_values= table->record[1]+alloc_length;
  }
  table->setup_tmp_table_column_bitmaps(bitmaps);

  recinfo= start_recinfo;
  null_flags=(unsigned char*) table->record[0];
  pos=table->record[0]+ null_pack_length;
  if (null_pack_length)
  {
    memset(recinfo, 0, sizeof(*recinfo));
    recinfo->type=FIELD_NORMAL;
    recinfo->length=null_pack_length;
    recinfo++;
    memset(null_flags, 255, null_pack_length);  // Set null fields

    table->null_flags= (unsigned char*) table->record[0];
    share->null_fields= null_count;
    share->null_bytes= null_pack_length;
  }
  null_count=1;

  {
    //Field *field= *reg_field;
    uint32_t length;
    memset(recinfo, 0, sizeof(*recinfo));
    field->move_field(pos,(unsigned char*) 0,0);

    field->reset();
    /*
      Test if there is a default field value. The test for ->ptr is to skip
      'offset' fields generated by initalize_tables
    */
    // Initialize the table field:
    memset(field->ptr, 0, field->pack_length());

    length=field->pack_length();
    pos+= length;

    /* Make entry for create table */
    recinfo->length=length;
    if (field->flags & BLOB_FLAG)
      recinfo->type= (int) FIELD_BLOB;
    else
      recinfo->type=FIELD_NORMAL;

    field->table_name= &table->alias;
  }

  //param->recinfo=recinfo;
  //store_record(table,s->default_values);        // Make empty default record

  if (session->variables.tmp_table_size == ~ (uint64_t) 0)    // No limit
    share->max_rows= ~(ha_rows) 0;
  else
    share->max_rows= (ha_rows) (((share->db_type() == heap_hton) ?
                                 cmin(session->variables.tmp_table_size,
                                      session->variables.max_heap_table_size) :
                                 session->variables.tmp_table_size) /
                                share->reclength);
  set_if_bigger(share->max_rows,1);    // For dummy start options


  //// keyinfo= param->keyinfo;
  if (true)
  {
    share->keys=1;
    share->uniques= test(using_unique_constraint);
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME;
    keyinfo->usable_key_parts= keyinfo->key_parts= 1;
    keyinfo->key_length=0;
    keyinfo->rec_per_key=0;
    keyinfo->algorithm= HA_KEY_ALG_UNDEF;
    keyinfo->name= (char*) "weedout_key";
    {
      key_part_info->null_bit=0;
      key_part_info->field=  field;
      key_part_info->offset= field->offset(table->record[0]);
      key_part_info->length= (uint16_t) field->key_length();
      key_part_info->type=   (uint8_t) field->key_type();
      key_part_info->key_type = FIELDFLAG_BINARY;
      if (!using_unique_constraint)
      {
        if (!(key_field= field->new_key_field(session->mem_root, table,
                                              group_buff,
                                              field->null_ptr,
                                              field->null_bit)))
          goto err;
        key_part_info->key_part_flag|= HA_END_SPACE_ARE_EQUAL; //todo need this?
      }
      keyinfo->key_length+=  key_part_info->length;
    }
  }

  if (session->is_fatal_error)        // If end of memory
    goto err;
  share->db_record_offset= 1;
  if (share->db_type() == myisam_hton)
  {
    recinfo++;
    if (table->create_myisam_tmp_table(keyinfo, start_recinfo, &recinfo, 0))
      goto err;
  }
  sjtbl->start_recinfo= start_recinfo;
  sjtbl->recinfo=       recinfo;
  if (table->open_tmp_table())
    goto err;

  session->mem_root= mem_root_save;
  return(table);

err:
  session->mem_root= mem_root_save;
  table->free_tmp_table(session);                    /* purecov: inspected */
  if (temp_pool_slot != MY_BIT_NONE)
    bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
  return(NULL);        /* purecov: inspected */
}
