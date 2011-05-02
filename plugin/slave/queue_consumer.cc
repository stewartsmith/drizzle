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
#include <plugin/slave/queue_consumer.h>
#include <drizzled/message/transaction.pb.h>
#include <drizzled/message/statement_transform.h>
#include <drizzled/sql/result_set.h>
#include <drizzled/execute.h>
#include <string>
#include <vector>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <google/protobuf/text_format.h>

using namespace std;
using namespace drizzled;

namespace slave
{

bool QueueConsumer::init()
{
  setApplierState("", true);
  return true;
}


void QueueConsumer::shutdown()
{
  setApplierState(getErrorMessage(), false);
}


bool QueueConsumer::process()
{
  TrxIdList completedTransactionIds;

  getListOfCompletedTransactions(completedTransactionIds);

  for (size_t x= 0; x < completedTransactionIds.size(); x++)
  {
    string commit_id;
    string originating_server_uuid;
    uint64_t originating_commit_id= 0;
    uint64_t trx_id= completedTransactionIds[x];

    vector<string> aggregate_sql;  /* final SQL to execute */
    vector<string> segmented_sql;  /* carryover from segmented statements */

    message::Transaction transaction;
    uint32_t segment_id= 1;

    while (getMessage(transaction, commit_id, trx_id, originating_server_uuid,
                      originating_commit_id, segment_id++))
    {
      convertToSQL(transaction, aggregate_sql, segmented_sql);
      transaction.Clear();
    }

    /*
     * The last message in a transaction should always have a commit_id
     * value larger than 0, though other messages of the same transaction
     * will have commit_id = 0.
     */
    assert((not commit_id.empty()) && (commit_id != "0"));
    assert(segmented_sql.empty());

    if (not aggregate_sql.empty())
    {
      /*
       * Execution using drizzled::Execute requires some special escaping.
       */
      vector<string>::iterator agg_iter;
      for (agg_iter= aggregate_sql.begin(); agg_iter != aggregate_sql.end(); ++agg_iter)
      {
        string &sql= *agg_iter;
        string::iterator si= sql.begin();
        for (; si != sql.end(); ++si)
        {
          if (*si == '\"')
          {
            si= sql.insert(si, '\\');
            ++si;
          }
          else if (*si == '\\')
          {
            si= sql.insert(si, '\\');
            ++si;
            si= sql.insert(si, '\\');
            ++si;
            si= sql.insert(si, '\\');
            ++si;
          }
          else if (*si == ';')
          {
            si= sql.insert(si, '\\');
            ++si;  /* advance back to the semicolon */
          }
        }
      }
    }

    if (not executeSQLWithCommitId(aggregate_sql, commit_id, 
                                   originating_server_uuid, 
                                   originating_commit_id))
    {
      return false;
    }

    if (not deleteFromQueue(trx_id))
    {
      return false;
    }
  }

  return true;
}


bool QueueConsumer::getMessage(message::Transaction &transaction,
                              string &commit_id,
                              uint64_t trx_id,
                              string &originating_server_uuid,
                              uint64_t &originating_commit_id,
                              uint32_t segment_id)
{
  string sql("SELECT `msg`, `commit_order`, `originating_server_uuid`, "
             "`originating_commit_id` FROM `sys_replication`.`queue`"
             " WHERE `trx_id` = ");
  sql.append(boost::lexical_cast<string>(trx_id));
  sql.append(" AND `seg_id` = ", 16);
  sql.append(boost::lexical_cast<string>(segment_id));

  sql::ResultSet result_set(4);
  Execute execute(*(_session.get()), true);
  
  execute.run(sql, result_set);
  
  assert(result_set.getMetaData().getColumnCount() == 4);

  /* Really should only be 1 returned row */
  uint32_t found_rows= 0;
  while (result_set.next())
  {
    string msg= result_set.getString(0);
    string com_id= result_set.getString(1);
    string orig_server_uuid= result_set.getString(2);
    string orig_commit_id= result_set.getString(3);

    if ((msg == "") || (found_rows == 1))
      break;

    /* No columns should be NULL */
    assert(result_set.isNull(0) == false);
    assert(result_set.isNull(1) == false);
    assert(result_set.isNull(2) == false);
    assert(result_set.isNull(3) == false);


    google::protobuf::TextFormat::ParseFromString(msg, &transaction);

    commit_id= com_id;
    originating_server_uuid= orig_server_uuid;
    originating_commit_id= boost::lexical_cast<uint64_t>(orig_commit_id);
    found_rows++;
  }

  if (found_rows == 0)
    return false;
  
  return true;
}

bool QueueConsumer::getListOfCompletedTransactions(TrxIdList &list)
{
  Execute execute(*(_session.get()), true);
  
  string sql("SELECT `trx_id` FROM `sys_replication`.`queue`"
             " WHERE `commit_order` IS NOT NULL AND `commit_order` > 0"
             " ORDER BY `commit_order` ASC");
  
  /* ResultSet size must match column count */
  sql::ResultSet result_set(1);

  execute.run(sql, result_set);

  assert(result_set.getMetaData().getColumnCount() == 1);

  while (result_set.next())
  {
    assert(result_set.isNull(0) == false);
    string value= result_set.getString(0);
    
    /* empty string returned when no more results */
    if (value != "")
    {
      list.push_back(boost::lexical_cast<uint64_t>(result_set.getString(0)));
    }
  }
  
  return true;
}


bool QueueConsumer::convertToSQL(const message::Transaction &transaction,
                                vector<string> &aggregate_sql,
                                vector<string> &segmented_sql)
{
  if (transaction.has_event())
    return true;

  size_t num_statements= transaction.statement_size();

  /*
   * Loop through all Statement messages within this Transaction and
   * convert each to equivalent SQL statements. Complete Statements will
   * be appended to aggregate_sql, while segmented Statements will remain
   * in segmented_sql to be appended to until completed, or rolled back.
   */

  for (size_t idx= 0; idx < num_statements; idx++)
  {
    const message::Statement &statement= transaction.statement(idx);
    
    /* We won't bother with executing a rolled back transaction */
    if (statement.type() == message::Statement::ROLLBACK)
    {
      assert(idx == (num_statements - 1));  /* should be the final Statement */
      aggregate_sql.clear();
      segmented_sql.clear();
      break;
    }

    switch (statement.type())
    {
      /* DDL cannot be in a transaction, so precede with a COMMIT */
      case message::Statement::TRUNCATE_TABLE:
      case message::Statement::CREATE_SCHEMA:
      case message::Statement::ALTER_SCHEMA:
      case message::Statement::DROP_SCHEMA:
      case message::Statement::CREATE_TABLE:
      case message::Statement::ALTER_TABLE:
      case message::Statement::DROP_TABLE:
      case message::Statement::RAW_SQL:  /* currently ALTER TABLE or RENAME */
      {
        segmented_sql.push_back("COMMIT");
        break;
      }

      /* Cancel any ongoing statement */
      case message::Statement::ROLLBACK_STATEMENT:
      {
        segmented_sql.clear();
        continue;
      }
      
      default:
      {
        break;
      }
    }

    if (message::transformStatementToSql(statement, segmented_sql,
                                         message::DRIZZLE, true))
    {
      return false;
    }

    if (isEndStatement(statement))
    {
      aggregate_sql.insert(aggregate_sql.end(),
                           segmented_sql.begin(),
                           segmented_sql.end());
      segmented_sql.clear();
    }
  }

  return true;
}


bool QueueConsumer::isEndStatement(const message::Statement &statement)
{
  switch (statement.type())
  {
    case (message::Statement::INSERT):
    {
      const message::InsertData &data= statement.insert_data();
      if (not data.end_segment())
        return false;
      break;
    }
    case (message::Statement::UPDATE):
    {
      const message::UpdateData &data= statement.update_data();
      if (not data.end_segment())
        return false;
      break;
    }
    case (message::Statement::DELETE):
    {
      const message::DeleteData &data= statement.delete_data();
      if (not data.end_segment())
        return false;
      break;
    }
    default:
      return true;
  }
  return true;
}


void QueueConsumer::setApplierState(const string &err_msg, bool status)
{
  vector<string> statements;
  string sql;
  string msg(err_msg);

  if (not status)
  {
    sql= "UPDATE `sys_replication`.`applier_state` SET `status` = 'STOPPED'";
  }
  else
  {
    sql= "UPDATE `sys_replication`.`applier_state` SET `status` = 'RUNNING'";
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


bool QueueConsumer::executeSQLWithCommitId(vector<string> &sql,
                                           const string &commit_id, 
                                           const string &originating_server_uuid, 
                                           uint64_t originating_commit_id)
{
  string tmp("UPDATE `sys_replication`.`applier_state`"
             " SET `last_applied_commit_id` = ");
  tmp.append(commit_id);
  tmp.append(", `originating_server_uuid` = '");
  tmp.append(originating_server_uuid);
  tmp.append("' , `originating_commit_id` = ");
  tmp.append(boost::lexical_cast<string>(originating_commit_id));

  sql.push_back(tmp);
 
  _session->setOriginatingServerUUID(originating_server_uuid);
  _session->setOriginatingCommitID(originating_commit_id);
 
  return executeSQL(sql);
}


bool QueueConsumer::deleteFromQueue(uint64_t trx_id)
{
  string sql("DELETE FROM `sys_replication`.`queue` WHERE `trx_id` = ");
  sql.append(boost::lexical_cast<std::string>(trx_id));

  vector<string> sql_vect;
  sql_vect.push_back(sql);

  return executeSQL(sql_vect);
}

} /* namespace slave */
