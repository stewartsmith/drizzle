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

#include <config.h>
#include <plugin/slave/queue_producer.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/sql/result_set.h>
#include <drizzled/execute.h>
#include <drizzled/gettext.h>
#include <drizzled/message/transaction.pb.h>
#include <boost/lexical_cast.hpp>
#include <google/protobuf/text_format.h>

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
  setIOState("", true);
  return reconnect(true);
}

bool QueueProducer::process()
{
  if (_saved_max_commit_id == 0)
  {
    if (not queryForMaxCommitId(&_saved_max_commit_id))
    {
      if (_last_return == DRIZZLE_RETURN_LOST_CONNECTION)
      {
        if (reconnect(false))
        {
          return true;    /* reconnect successful, try again */
        }
        else
        {
          _last_error_message= "Master offline";
          return false;   /* reconnect failed, shutdown the thread */
        }
      }
      else
      {
        return false;     /* unrecoverable error, shutdown the thread */
      }
    }
  }

  /* Keep getting events until caught up */
  enum drizzled::error_t err;
  while ((err= (queryForReplicationEvents(_saved_max_commit_id))) == EE_OK)
  {}

  if (err == ER_YES)  /* We encountered an error */
  {
    if (_last_return == DRIZZLE_RETURN_LOST_CONNECTION)
    {
      if (reconnect(false))
      {
        return true;    /* reconnect successful, try again */
      }
      else
      {
        _last_error_message= "Master offline";
        return false;   /* reconnect failed, shutdown the thread */
      }
    }
    else
    {
      return false;     /* unrecoverable error, shutdown the thread */
    }
  }

  return true;
}

void QueueProducer::shutdown()
{
  setIOState(_last_error_message, false);
  if (_is_connected)
    closeConnection();
}

bool QueueProducer::reconnect(bool initial_connection)
{
  if (not initial_connection)
  {
    errmsg_printf(error::ERROR, _("Lost connection to master. Reconnecting."));
  }

  _is_connected= false;
  _last_return= DRIZZLE_RETURN_OK;
  _last_error_message.clear();
  boost::posix_time::seconds duration(_seconds_between_reconnects);

  uint32_t attempts= 1;

  while (not openConnection())
  {
    if (attempts++ == _max_reconnects)
      break;
    boost::this_thread::sleep(duration);
  }

  return _is_connected;
}

bool QueueProducer::openConnection()
{
  if (drizzle_create(&_drizzle) == NULL)
  {
    _last_return= DRIZZLE_RETURN_INTERNAL_ERROR;
    _last_error_message= "Replication slave: ";
    _last_error_message.append(drizzle_error(&_drizzle));
    errmsg_printf(error::ERROR, _("%s"), _last_error_message.c_str());
    return false;
  }
  
  if (drizzle_con_create(&_drizzle, &_connection) == NULL)
  {
    _last_return= DRIZZLE_RETURN_INTERNAL_ERROR;
    _last_error_message= "Replication slave: ";
    _last_error_message.append(drizzle_error(&_drizzle));
    errmsg_printf(error::ERROR, _("%s"), _last_error_message.c_str());
    return false;
  }
  
  drizzle_con_set_tcp(&_connection, _master_host.c_str(), _master_port);
  drizzle_con_set_auth(&_connection, _master_user.c_str(), _master_pass.c_str());

  drizzle_return_t ret= drizzle_con_connect(&_connection);

  if (ret != DRIZZLE_RETURN_OK)
  {
    _last_return= ret;
    _last_error_message= "Replication slave: ";
    _last_error_message.append(drizzle_error(&_drizzle));
    errmsg_printf(error::ERROR, _("%s"), _last_error_message.c_str());
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
    _last_return= ret;
    drizzle_result_free(&result);
    return false;
  }

  drizzle_result_free(&result);

  return true;
}

bool QueueProducer::queryForMaxCommitId(uint64_t *max_commit_id)
{
  /*
   * This SQL will get the maximum commit_id value we have pulled over from
   * the master. We query two tables because either the queue will be empty,
   * in which case the last_applied_commit_id will be the value we want, or
   * we have yet to drain the queue,  we get the maximum value still in
   * the queue.
   */
  string sql("SELECT MAX(x.cid) FROM"
             " (SELECT MAX(`commit_order`) AS cid FROM `sys_replication`.`queue`"
             "  UNION ALL SELECT `last_applied_commit_id` AS cid"
             "  FROM `sys_replication`.`applier_state`) AS x");

  sql::ResultSet result_set(1);
  Execute execute(*(_session.get()), true);
  execute.run(sql, result_set);
  assert(result_set.getMetaData().getColumnCount() == 1);

  /* Really should only be 1 returned row */
  uint32_t found_rows= 0;
  while (result_set.next())
  {
    string value= result_set.getString(0);

    if ((value == "") || (found_rows == 1))
      break;

    assert(result_set.isNull(0) == false);
    *max_commit_id= boost::lexical_cast<uint64_t>(value);
    found_rows++;
  }

  if (found_rows == 0)
  {
    _last_error_message= "Could not determine last committed transaction.";
    return false;
  }

  return true;
}

bool QueueProducer::queryForTrxIdList(uint64_t max_commit_id,
                                      vector<uint64_t> &list)
{
  (void)list;
  string sql("SELECT `id` FROM `data_dictionary`.`sys_replication_log`"
             " WHERE `commit_id` > ");
  sql.append(boost::lexical_cast<string>(max_commit_id));
  sql.append(" ORDER BY `commit_id` LIMIT 25");

  drizzle_return_t ret;
  drizzle_result_st result;
  drizzle_query_str(&_connection, &result, sql.c_str(), &ret);
  
  if (ret != DRIZZLE_RETURN_OK)
  {
    _last_return= ret;
    _last_error_message= "Replication slave: ";
    _last_error_message.append(drizzle_error(&_drizzle));
    errmsg_printf(error::ERROR, _("%s"), _last_error_message.c_str());
    drizzle_result_free(&result);
    return false;
  }

  ret= drizzle_result_buffer(&result);

  if (ret != DRIZZLE_RETURN_OK)
  {
    _last_return= ret;
    _last_error_message= "Replication slave: ";
    _last_error_message.append(drizzle_error(&_drizzle));
    errmsg_printf(error::ERROR, _("%s"), _last_error_message.c_str());
    drizzle_result_free(&result);
    return false;
  }

  drizzle_row_t row;

  while ((row= drizzle_row_next(&result)) != NULL)
  {
    if (row[0])
    {
      list.push_back(boost::lexical_cast<uint32_t>(row[0]));
    }
    else
    {
      _last_return= ret;
      _last_error_message= "Replication slave: Unexpected NULL for trx id";
      errmsg_printf(error::ERROR, _("%s"), _last_error_message.c_str());
      drizzle_result_free(&result);
      return false;
    }
  }

  drizzle_result_free(&result);
  return true;
}


bool QueueProducer::queueInsert(const char *trx_id,
                                const char *seg_id,
                                const char *commit_id,
                                const char *originating_server_uuid,
                                const char *originating_commit_id,
                                const char *msg,
                                const char *msg_length)
{
  message::Transaction message;

  message.ParseFromArray(msg, boost::lexical_cast<int>(msg_length));

  /*
   * The SQL to insert our results into the local queue.
   */
  string sql= "INSERT INTO `sys_replication`.`queue`"
              " (`trx_id`, `seg_id`, `commit_order`,"
              "  `originating_server_uuid`, `originating_commit_id`, `msg`) VALUES (";
  sql.append(trx_id);
  sql.append(", ", 2);
  sql.append(seg_id);
  sql.append(", ", 2);
  sql.append(commit_id);
  sql.append(", '", 3);
  sql.append(originating_server_uuid);
  sql.append("' , ", 4);
  sql.append(originating_commit_id);
  sql.append(", '", 3);

  /*
   * Ideally we would store the Transaction message in binary form, as it
   * it stored on the master and tranferred to the slave. However, we are
   * inserting using drizzle::Execute which doesn't really handle binary
   * data. Until that is changed, we store as plain text.
   */
  string message_text;
  google::protobuf::TextFormat::PrintToString(message, &message_text);  

  /*
   * Execution using drizzled::Execute requires some special escaping.
   */
  string::iterator it= message_text.begin();
  for (; it != message_text.end(); ++it)
  {
    if (*it == '\"')
    {
      it= message_text.insert(it, '\\');
      ++it;
    }
    else if (*it == '\'')
    {
      it= message_text.insert(it, '\\');
      ++it;
      it= message_text.insert(it, '\\');
      ++it;
    }
    else if (*it == '\\')
    {
      it= message_text.insert(it, '\\');
      ++it;
      it= message_text.insert(it, '\\');
      ++it;
      it= message_text.insert(it, '\\');
      ++it;
    }
    else if (*it == ';')
    {
      it= message_text.insert(it, '\\');
      ++it;  /* advance back to the semicolon */
    }
  }

  sql.append(message_text);
  sql.append("')", 2);

  vector<string> statements;
  statements.push_back(sql);

  if (not executeSQL(statements))
  {
    markInErrorState();
    return false;
  }

  uint64_t tmp_commit_id= boost::lexical_cast<uint64_t>(commit_id);
  if (tmp_commit_id > _saved_max_commit_id)
    _saved_max_commit_id= tmp_commit_id;

  return true;
}


enum drizzled::error_t QueueProducer::queryForReplicationEvents(uint64_t max_commit_id)
{
  vector<uint64_t> trx_id_list;

  if (not queryForTrxIdList(max_commit_id, trx_id_list))
    return ER_YES;

  if (trx_id_list.size() == 0)    /* nothing to get from the master */
  {
    return ER_NO;
  }

  /*
   * The SQL to pull everything we need from the master.
   */
  string sql= "SELECT `id`, `segid`, `commit_id`, `originating_server_uuid`,"
              " `originating_commit_id`, `message`, `message_len` "
              " FROM `data_dictionary`.`sys_replication_log` WHERE `id` IN (";

  for (size_t x= 0; x < trx_id_list.size(); x++)
  {
    if (x > 0)
      sql.append(", ", 2);
    sql.append(boost::lexical_cast<string>(trx_id_list[x]));
  }

  sql.append(")", 1);
  sql.append(" ORDER BY `commit_id` ASC");

  drizzle_return_t ret;
  drizzle_result_st result;
  drizzle_query_str(&_connection, &result, sql.c_str(), &ret);
  
  if (ret != DRIZZLE_RETURN_OK)
  {
    _last_return= ret;
    _last_error_message= "Replication slave: ";
    _last_error_message.append(drizzle_error(&_drizzle));
    errmsg_printf(error::ERROR, _("%s"), _last_error_message.c_str());
    drizzle_result_free(&result);
    return ER_YES;
  }

  /* TODO: Investigate 1-row-at-a-time buffering */

  ret= drizzle_result_buffer(&result);

  if (ret != DRIZZLE_RETURN_OK)
  {
    _last_return= ret;
    _last_error_message= "Replication slave: ";
    _last_error_message.append(drizzle_error(&_drizzle));
    errmsg_printf(error::ERROR, _("%s"), _last_error_message.c_str());
    drizzle_result_free(&result);
    return ER_YES;
  }

  drizzle_row_t row;

  while ((row= drizzle_row_next(&result)) != NULL)
  {
    if (not queueInsert(row[0], row[1], row[2], row[3], row[4], row[5], row[6]))
    {
      errmsg_printf(error::ERROR,
                    _("Replication slave: Unable to insert into queue."));
      drizzle_result_free(&result);
      return ER_YES;
    }
  }

  drizzle_result_free(&result);

  return EE_OK;
}


void QueueProducer::setIOState(const string &err_msg, bool status)
{
  vector<string> statements;
  string sql;
  string msg(err_msg);

  if (not status)
  {
    sql= "UPDATE `sys_replication`.`io_state` SET `status` = 'STOPPED'";
  }
  else
  {
    sql= "UPDATE `sys_replication`.`io_state` SET `status` = 'RUNNING'";
  }
  
  sql.append(", `error_msg` = '", 17);

  /* Escape embedded quotes and statement terminators */
  string::iterator it;
  for (it= msg.begin(); it != msg.end(); ++it)
  {
    if (*it == '\'')
    {
      it= msg.insert(it, '\'');
      ++it;  /* advance back to the quote */
    }
    else if (*it == ';')
    {
      it= msg.insert(it, '\\');
      ++it;  /* advance back to the semicolon */
    }
  }
  
  sql.append(msg);
  sql.append("'", 1);

  statements.push_back(sql);
  executeSQL(statements);
}

} /* namespace slave */
