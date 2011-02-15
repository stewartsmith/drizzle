/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 David Shrewsbury
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
#include "plugin/slave/queue_producer.h"
#include <drizzled/errmsg_print.h>
#include <drizzled/gettext.h>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace drizzled;

namespace slave
{

QueueProducer::~QueueProducer()
{
  if (_is_connected)
    closeConnection();
}

bool QueueProducer::init()
{
  return openConnection();
}

/* TODO: Add ability to retry after an error */
bool QueueProducer::process()
{
  if (_saved_max_commit_id == 0)
  {
    if (not queryForMaxCommitId(&_saved_max_commit_id))
    {
      return false;
    }
  }

  if (not queryForReplicationEvents(_saved_max_commit_id))
  {
    return false;
  }

  return true;
}

void QueueProducer::shutdown()
{
  if (_is_connected)
    closeConnection();
}

bool QueueProducer::openConnection()
{
  drizzle_return_t ret;

  if (drizzle_create(&_drizzle) == NULL)
  {
    errmsg_printf(error::ERROR,
                  _("Replication slave: Error during drizzle_create()\n"));
    return false;
  }
  
  if (drizzle_con_create(&_drizzle, &_connection) == NULL)
  {
    errmsg_printf(error::ERROR,
                  _("Replication slave: %s\n"),
                  drizzle_error(&_drizzle));
    return false;
  }
  
  drizzle_con_set_tcp(&_connection, _master_host.c_str(), _master_port);
  drizzle_con_set_auth(&_connection, _master_user.c_str(), _master_pass.c_str());

  ret= drizzle_con_connect(&_connection);

  if (ret != DRIZZLE_RETURN_OK)
  {
    errmsg_printf(error::ERROR,
                  _("Replication slave: %s\n"),
                  drizzle_error(&_drizzle));
    return false;
  }
  
  _is_connected= true;

  return true;
}

bool QueueProducer::closeConnection()
{
  drizzle_return_t ret;
  drizzle_result_st result;

  _is_connected= false;

  if (drizzle_quit(&_connection, &result, &ret) == NULL)
  {
    return false;
  }

  drizzle_result_free(&result);

  return true;
}

bool QueueProducer::queryForMaxCommitId(uint32_t *max_commit_id)
{
  /*
   * This SQL will get the maximum commit_id value we have pulled over from
   * the master. We query two tables because either the queue will be empty,
   * in which case the last_applied_commit_id will be the value we want, or
   * we have yet to drain the queue,  we get the maximum value still in
   * the queue.
   */
  string sql("SELECT MAX(x.cid) FROM"
             " (SELECT MAX(commit_order) AS cid FROM replication.queue"
             "  UNION ALL SELECT last_applied_commit_id AS cid"
             "  FROM replication.applier_state) AS x");

  (void)*max_commit_id;
  return true;
}

bool QueueProducer::queryForReplicationEvents(uint32_t max_commit_id)
{
  //vector<uint64_t> trx_id_list;
  (void)max_commit_id;

  // SELECT id FROM data_dictionary.sys_replication_log WHERE commit_id > ?

  string select_sql("SELECT id, segid, commit_id, message "
                    " FROM data_dictionary.sys_replication_log WHERE id IN (");
  // append from trx_id_list
  select_sql.append(")");

  string insert_sql("INSERT INTO replication.queue"
                    " (trx_id, seg_id, commit_order, msg) VALUES (");
  // append from results above
  insert_sql.append(")");

  drizzle_return_t ret;
  drizzle_result_st result;
  drizzle_result_create(&_connection, &result);
  drizzle_query_str(&_connection, &result, select_sql.c_str(), &ret);
  
  if (ret != DRIZZLE_RETURN_OK)
  {
    errmsg_printf(error::ERROR,
                  _("Replication slave: %s\n"),
                  drizzle_error(&_drizzle));
    return false;
  }

  printf("queryForMaxCommitId()\n");
  printf("Result:     row_count=%" PRId64 "\n"
         "            insert_id=%" PRId64 "\n"
         "        warning_count=%u\n"
         "         column_count=%u\n"
         "        affected_rows=%" PRId64 "\n\n",
         drizzle_result_row_count(&result),
         drizzle_result_insert_id(&result),
         drizzle_result_warning_count(&result),
         drizzle_result_column_count(&result),
         drizzle_result_affected_rows(&result));
  fflush(stdout);

  ret= drizzle_result_buffer(&result);

  if (ret != DRIZZLE_RETURN_OK)
  {
    errmsg_printf(error::ERROR,
                  _("Replication slave: %s\n"),
                  drizzle_error(&_drizzle));
    drizzle_result_free(&result);
    return false;
  }

  /* only 1 row to process */
  drizzle_row_t row;
  if ((row= drizzle_row_next(&result)) == NULL)
  {
    errmsg_printf(error::ERROR,
                  _("Replication slave: No results for max commit id\n"));
    drizzle_result_free(&result);
    return false;
  }

  if (row[0])
  {
    max_commit_id= boost::lexical_cast<uint32_t>(row[0]);
  }
  else
  {
    errmsg_printf(error::ERROR,
                  _("Replication slave: Unexpected NULL for max commit id\n"));
    drizzle_result_free(&result);
    return false;
  }

  drizzle_result_free(&result);
  return true;
}

} /* namespace slave */
