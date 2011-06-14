/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
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

#include <drizzled/message.h>
#include "read_replication.h"
#include "create_replication.h"

#ifdef UNIV_NONINL
#include "dict0crea.ic"
#endif

#include "btr0pcur.h"
#include "btr0btr.h"
#include "page0page.h"
#include "mach0data.h"
#include "dict0boot.h"
#include "dict0dict.h"
#include "que0que.h"
#include "row0ins.h"
#include "row0mysql.h"
#include "pars0pars.h"
#include "trx0roll.h"
#include "usr0sess.h"
#include "ut0vec.h"
#include "row0merge.h"
#include "row0mysql.h"

UNIV_INTERN ulint dict_create_sys_replication_log(void)
{
  dict_table_t*	table1;
  ulint error;
  trx_t *trx;

  mutex_enter(&(dict_sys->mutex));

  table1 = dict_table_get_low("SYS_REPLICATION_LOG");

  trx_sys_read_commit_id();

  if (table1) 
  {
    mutex_exit(&(dict_sys->mutex));

    return(DB_SUCCESS);
  }

  mutex_exit(&(dict_sys->mutex));

  trx= trx_allocate_for_mysql();

  trx->op_info= "creating replication sys table";

  row_mysql_lock_data_dictionary(trx);

  pars_info_t *info= pars_info_create();


  error = que_eval_sql(info,
                       "PROCEDURE CREATE_SYS_REPLICATION_LOG_PROC () IS\n"
                       "BEGIN\n"
                       "CREATE TABLE SYS_REPLICATION_LOG(ID INT(8), SEGID INT, COMMIT_ID INT(8), END_TIMESTAMP INT(8), ORIGINATING_SERVER_UUID BLOB, ORIGINATING_COMMIT_ID INT(8), MESSAGE_LEN INT, MESSAGE BLOB);\n" 
                       "CREATE UNIQUE CLUSTERED INDEX PRIMARY ON SYS_REPLICATION_LOG (ID, SEGID);\n"
                       "CREATE INDEX COMMIT_IDX ON SYS_REPLICATION_LOG (COMMIT_ID, ID);\n"
                       "END;\n"
                       , FALSE, trx);



  if (error != DB_SUCCESS)
  {
    fprintf(stderr, "InnoDB: error %lu in creation.\n", (ulong) error);

    ut_a(error == DB_OUT_OF_FILE_SPACE || error == DB_TOO_MANY_CONCURRENT_TRXS);

    fprintf(stderr,
            "InnoDB: creation failed\n"
            "InnoDB: tablespace is full\n"
            "InnoDB: dropping incompletely created SYS_REPLICATION_LOG table.\n");

    row_drop_table_for_mysql("SYS_REPLICATION_LOG", trx, TRUE);

    error = DB_MUST_GET_MORE_FILE_SPACE;
  }

  trx_commit_for_mysql(trx);

  row_mysql_unlock_data_dictionary(trx);

  trx_free_for_mysql(trx);

  return(error);
}

UNIV_INTERN int read_replication_log_table_message(const char* table_name, drizzled::message::Table *table_message)
{
  std::string search_string(table_name);
  boost::algorithm::to_lower(search_string);

  if (search_string.compare("sys_replication_log") != 0)
    return -1;

  drizzled::message::Engine *engine= table_message->mutable_engine();
  engine->set_name("InnoDB");
  table_message->set_name("SYS_REPLICATION_LOG");
  table_message->set_schema("DATA_DICTIONARY");
  table_message->set_type(drizzled::message::Table::STANDARD);
  table_message->set_creation_timestamp(0);
  table_message->set_update_timestamp(0);

  drizzled::message::Table::TableOptions *options= table_message->mutable_options();
  options->set_collation_id(drizzled::my_charset_bin.number);
  options->set_collation(drizzled::my_charset_bin.name);
  drizzled::message::set_is_replicated(*table_message, false);

  drizzled::message::Table::Field *field= table_message->add_field();
  field->set_name("ID");
  field->set_type(drizzled::message::Table::Field::BIGINT);

  field= table_message->add_field();
  field->set_name("SEGID");
  field->set_type(drizzled::message::Table::Field::INTEGER);

  field= table_message->add_field();
  field->set_name("COMMIT_ID");
  field->set_type(drizzled::message::Table::Field::BIGINT);

  field= table_message->add_field();
  field->set_name("END_TIMESTAMP");
  field->set_type(drizzled::message::Table::Field::BIGINT);

  field= table_message->add_field();
  field->set_name("ORIGINATING_SERVER_UUID");
  field->set_type(drizzled::message::Table::Field::BLOB);

  field= table_message->add_field();
  field->set_name("ORIGINATING_COMMIT_ID");
  field->set_type(drizzled::message::Table::Field::BIGINT);

  field= table_message->add_field();
  field->set_name("MESSAGE_LEN");
  field->set_type(drizzled::message::Table::Field::INTEGER);

  field= table_message->add_field();
  field->set_name("MESSAGE");
  field->set_type(drizzled::message::Table::Field::BLOB);
  drizzled::message::Table::Field::StringFieldOptions *stropt= field->mutable_string_options();
  stropt->set_collation_id(drizzled::my_charset_bin.number);
  stropt->set_collation(drizzled::my_charset_bin.name);

  drizzled::message::Table::Index *index= table_message->add_indexes();
  index->set_name("PRIMARY");
  index->set_is_primary(true);
  index->set_is_unique(true);
  index->set_type(drizzled::message::Table::Index::BTREE);
  index->set_key_length(12);
  drizzled::message::Table::Index::IndexPart *part= index->add_index_part();
  part->set_fieldnr(0);
  part->set_compare_length(8);
  part= index->add_index_part();
  part->set_fieldnr(1);
  part->set_compare_length(4);

  index= table_message->add_indexes();
  index->set_name("COMMIT_IDX");
  index->set_is_primary(false);
  index->set_is_unique(false);
  index->set_type(drizzled::message::Table::Index::BTREE);
  index->set_key_length(16);
  part= index->add_index_part();
  part->set_fieldnr(2);
  part->set_compare_length(8);
  part= index->add_index_part();
  part->set_fieldnr(0);
  part->set_compare_length(8);

  return 0;
}

extern dtuple_t* row_get_prebuilt_insert_row(row_prebuilt_t*	prebuilt);

ulint insert_replication_message(const char *message, size_t size, 
                                 trx_t *trx, uint64_t trx_id, 
                                 uint64_t end_timestamp, bool is_end_segment, 
                                 uint32_t seg_id, const char *server_uuid,
                                 bool use_originating_server_uuid,
                                 const char *originating_server_uuid,
                                 uint64_t originating_commit_id)
{
  ulint error;
  row_prebuilt_t*	prebuilt;	/* For reading rows */
  dict_table_t *table;
  que_thr_t*	thr;
  byte*  data;

  table = dict_table_get("SYS_REPLICATION_LOG",TRUE);

  prebuilt = row_create_prebuilt(table);

  if (prebuilt->trx != trx) 
  {
    row_update_prebuilt_trx(prebuilt, trx);
  }

  /* DDL operations create table/drop table call
   * innobase_commit_low() which will commit the trx
   * that leaves the operation of committing to the
   * log in a new trx. If that is the case we need
   * to keep track and commit the trx later in this
   * function. 
   */ 
  bool is_started= true;
  if (trx->conc_state == TRX_NOT_STARTED)
  {
    is_started= false;
  }

  dtuple_t* dtuple= row_get_prebuilt_insert_row(prebuilt);
  dfield_t *dfield;

  dfield = dtuple_get_nth_field(dtuple, 0);
  data= static_cast<byte*>(mem_heap_alloc(prebuilt->heap, 8));
  row_mysql_store_col_in_innobase_format(dfield, data, TRUE, (byte*)&trx_id, 8, dict_table_is_comp(prebuilt->table));
  dfield_set_data(dfield, data, 8);

  dfield = dtuple_get_nth_field(dtuple, 1);

  data= static_cast<byte*>(mem_heap_alloc(prebuilt->heap, 4));
  row_mysql_store_col_in_innobase_format(dfield, data, TRUE, (byte*)&seg_id, 4, dict_table_is_comp(prebuilt->table));
  dfield_set_data(dfield, data, 4);
  
  uint64_t commit_id= 0;
  if (is_end_segment)
  {
    commit_id= trx_sys_commit_id.increment();
  } 

  dfield = dtuple_get_nth_field(dtuple, 2);
  data= static_cast<byte*>(mem_heap_alloc(prebuilt->heap, 8));
  row_mysql_store_col_in_innobase_format(dfield, data, TRUE, (byte*)&commit_id, 8, dict_table_is_comp(prebuilt->table));
  dfield_set_data(dfield, data, 8);

  dfield = dtuple_get_nth_field(dtuple, 3);
  data= static_cast<byte*>(mem_heap_alloc(prebuilt->heap, 8));
  row_mysql_store_col_in_innobase_format(dfield, data, TRUE, (byte*)&end_timestamp, 8, dict_table_is_comp(prebuilt->table));
  dfield_set_data(dfield, data, 8);

  if (not use_originating_server_uuid)
  {
    /* This transaction originated from this server, rather then being
       replicated to this server reset the values to reflect that */
    originating_server_uuid= server_uuid;
    originating_commit_id= commit_id;
  }

  dfield = dtuple_get_nth_field(dtuple, 4);
  dfield_set_data(dfield, originating_server_uuid, 36);

  dfield = dtuple_get_nth_field(dtuple, 5);
  data= static_cast<byte*>(mem_heap_alloc(prebuilt->heap, 8));
  row_mysql_store_col_in_innobase_format(dfield, data, TRUE, (byte*)&originating_commit_id, 8, dict_table_is_comp(prebuilt->table));
  dfield_set_data(dfield, data, 8);

  dfield = dtuple_get_nth_field(dtuple, 6);
  data= static_cast<byte*>(mem_heap_alloc(prebuilt->heap, 4));
  row_mysql_store_col_in_innobase_format(dfield, data, TRUE, (byte*)&size, 4, dict_table_is_comp(prebuilt->table));
  dfield_set_data(dfield, data, 4);

  dfield = dtuple_get_nth_field(dtuple, 7);
  dfield_set_data(dfield, message, size);

  ins_node_t*	node		= prebuilt->ins_node;

  thr = que_fork_get_first_thr(prebuilt->ins_graph);

  if (prebuilt->sql_stat_start) {
    node->state = INS_NODE_SET_IX_LOCK;
    prebuilt->sql_stat_start = FALSE;
  } else {
    node->state = INS_NODE_ALLOC_ROW_ID;
  }

  que_thr_move_to_run_state_for_mysql(thr, trx);

//run_again:
  thr->run_node = node;
  thr->prev_node = node;

  row_ins_step(thr);

  error = trx->error_state;

  que_thr_stop_for_mysql_no_error(thr, trx);
  row_prebuilt_free(prebuilt, FALSE);

  if (! is_started)
  {
    trx_commit_for_mysql(trx);
  }

  return error;
}

UNIV_INTERN read_replication_state_st *replication_read_init(void)
{
  read_replication_state_st *state= new read_replication_state_st;

  mutex_enter(&(dict_sys->mutex));

  mtr_start(&state->mtr);
  state->sys_tables= dict_table_get_low("SYS_REPLICATION_LOG");
  state->sys_index= UT_LIST_GET_FIRST(state->sys_tables->indexes);

  mutex_exit(&(dict_sys->mutex));

  btr_pcur_open_at_index_side(TRUE, state->sys_index, BTR_SEARCH_LEAF, &state->pcur, TRUE, &state->mtr);

  return state;
}

UNIV_INTERN void replication_read_deinit(struct read_replication_state_st *state)
{
  btr_pcur_close(&state->pcur);
  mtr_commit(&state->mtr);
  delete state;
}

UNIV_INTERN struct read_replication_return_st replication_read_next(struct read_replication_state_st *state)
{
  struct read_replication_return_st ret;
  const rec_t *rec;

  btr_pcur_move_to_next_user_rec(&state->pcur, &state->mtr);

  rec= btr_pcur_get_rec(&state->pcur);

  while (btr_pcur_is_on_user_rec(&state->pcur))
  {
    const byte*	field;
    ulint len;

    // Is the row deleted? If so go fetch the next
    if (rec_get_deleted_flag(rec, 0))
      continue;

    // Store transaction id
    field = rec_get_nth_field_old(rec, 0, &len);
    byte idbyte[8];
    convert_to_mysql_format(idbyte, field, 8);
    ret.id= *(uint64_t *)idbyte;

    // Store segment id
    field = rec_get_nth_field_old(rec, 1, &len);
    byte segbyte[4];
    convert_to_mysql_format(segbyte, field, 4);
    ret.seg_id= *(uint32_t *)segbyte;

    field = rec_get_nth_field_old(rec, 4, &len);
    byte commitbyte[8];
    convert_to_mysql_format(commitbyte, field, 8);
    ret.commit_id= *(uint64_t *)commitbyte;

    field = rec_get_nth_field_old(rec, 5, &len);
    byte timestampbyte[8];
    convert_to_mysql_format(timestampbyte, field, 8);
    ret.end_timestamp= *(uint64_t *)timestampbyte;

    field = rec_get_nth_field_old(rec, 6, &len);
    ret.originating_server_uuid= (char *)field;

    field = rec_get_nth_field_old(rec, 7, &len);
    byte originatingcommitbyte[8];
    convert_to_mysql_format(originatingcommitbyte, field, 8);
    ret.originating_commit_id= *(uint64_t *)originatingcommitbyte;

    // Handler message
    field = rec_get_nth_field_old(rec, 9, &len);
    ret.message= (char *)field;
    ret.message_length= len;

    // @todo double check that "field" will continue to be value past this
    // point.
    btr_pcur_store_position(&state->pcur, &state->mtr);
    mtr_commit(&state->mtr);

    mtr_start(&state->mtr);

    btr_pcur_restore_position(BTR_SEARCH_LEAF, &state->pcur, &state->mtr);

    return ret;
  }

  /* end of index */
  memset(&ret, 0, sizeof(ret));

  return ret;
}

UNIV_INTERN void convert_to_mysql_format(byte* out, const byte* in, int len)
{
  byte *ptr;
  ptr = out + len;

  for (;;) {
    ptr--;
    *ptr = *in;
    if (ptr == out) {
      break;
    }
    in++;
  }

  out[len - 1] = (byte) (out[len - 1] ^ 128);

}
