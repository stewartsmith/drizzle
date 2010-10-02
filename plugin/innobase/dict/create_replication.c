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
                       "CREATE TABLE SYS_REPLICATION_LOG(ID INT, MESSAGE BLOB);\n"
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

ulint insert_replication_message(const char *message, size_t size, trx_t *trx)
{
  ulint error;
  pars_info_t *info= pars_info_create();

  pars_info_add_dulint_literal(info, "ID", trx->id);
  pars_info_add_literal(info, "MESSAGE", message, size, DATA_FIXBINARY, DATA_ENGLISH);

  error= que_eval_sql(info,
                      "PROCEDURE P () IS\n"
                      "BEGIN\n"
                      "INSERT INTO SYS_REPLICATION_LOG VALUES (:ID, :MESSAGE);\n"
                      "END;\n",
                      FALSE, trx);

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
    dulint du_transaction_id= mtr_read_dulint(field, &state->mtr);
    ret.id= (ib_uint64_t) ut_conv_dulint_to_longlong(du_transaction_id);

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
