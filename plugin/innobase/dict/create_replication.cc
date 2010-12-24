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

#include "config.h"

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

UNIV_INTERN ulint dict_create_sys_replication_log(void)
{
  dict_table_t*	table1;
  ulint error;
  trx_t *trx;

  mutex_enter(&(dict_sys->mutex));

  table1 = dict_table_get_low("SYS_REPLICATION_LOG");

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
                       "CREATE TABLE SYS_REPLICATION_LOG(ID BINARY(8), MESSAGE BLOB);\n"
                       "CREATE UNIQUE CLUSTERED INDEX ID_IND ON SYS_REPLICATION_LOG (ID);\n"
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

extern dtuple_t* row_get_prebuilt_insert_row(row_prebuilt_t*	prebuilt);

ulint insert_replication_message(const char *message, size_t size, 
                                 trx_t *trx, uint64_t trx_id)
{
  ulint error;
  row_prebuilt_t*	prebuilt;	/* For reading rows */
  dict_table_t *table;
  que_thr_t*	thr;

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

  dfield_set_data(dfield, &trx_id, 8);

  dfield = dtuple_get_nth_field(dtuple, 1);
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

UNIV_INTERN struct read_replication_state_st *replication_read_init(void)
{
  struct read_replication_state_st *state= calloc(1, sizeof(struct read_replication_state_st));

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
  free(state);
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
    ret.id= *(uint64_t *)field;

    // Handler message
    field = rec_get_nth_field_old(rec, 3, &len);
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
