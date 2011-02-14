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
#include "drizzled/errmsg_print.h"
#include "drizzled/gettext.h"

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

bool QueueProducer::process()
{
  // SLAVE QUERIES

  /*
  1. SELECT MAX(commit_order) FROM replication.queue
  
  2. If nothing from #1: SELECT last_applied_commit_id FROM replication.applier_state

  5. INSERT INTO replication.queue (trx_id, seg_id, commit_order, msg)
     VALUES (?, ?, ?, ?)
   */

  // MASTER QUERIES

  /*
  3. SELECT id FROM data_dictionary.sys_replication_log
     WHERE commit_id > ?

  4. SELECT id, segid, commit_id, message
     FROM data_dictionary.sys_replication_log
     WHERE id IN (?)
   */

  string id_query("");

  drizzle_return_t ret;
  drizzle_result_st result;
  drizzle_result_create(&connection, &result);
  drizzle_query_str(&connection, &result, id_query.c_str(), &ret);
  
  if (ret != DRIZZLE_RETURN_OK)
  {
    errmsg_printf(error::ERROR,
                  _("Replication slave: %s\n"),
                  drizzle_error(&drizzle));
    /* TODO: put in a retry or similar */
    return false;
  }
  
  /* Get list of completed transaction ids from master where commit_id > (max(commit_id) || last_applied_commit_id) */
  /* Select from master where trx_id in list from above */
  /* Insert into local queue */
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

  if (drizzle_create(&drizzle) == NULL)
  {
    errmsg_printf(error::ERROR,
                  _("Replication slave: Error during drizzle_create()\n"));
    return false;
  }
  
  if (drizzle_con_create(&drizzle, &connection) == NULL)
  {
    errmsg_printf(error::ERROR,
                  _("Replication slave: %s\n"),
                  drizzle_error(&drizzle));
    return false;
  }
  
  drizzle_con_set_tcp(&connection, _master_host.c_str(), _master_port);
  drizzle_con_set_auth(&connection, _master_user.c_str(), _master_pass.c_str());

  ret= drizzle_con_connect(&connection);

  if (ret != DRIZZLE_RETURN_OK)
  {
    errmsg_printf(error::ERROR,
                  _("Replication slave: %s\n"),
                  drizzle_error(&drizzle));
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

  if (drizzle_quit(&connection, &result, &ret) == NULL)
  {
    return false;
  }

  drizzle_result_free(&result);

  return true;
}

} /* namespace slave */
