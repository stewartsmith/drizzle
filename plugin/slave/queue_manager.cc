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
#include "plugin/slave/queue_manager.h"
#include "drizzled/message/transaction.pb.h"
#include "drizzled/message/statement_transform.h"
#include "drizzled/plugin/listen.h"
#include "drizzled/plugin/client.h"
#include "drizzled/catalog/local.h"
#include "drizzled/execute.h"
#include "drizzled/internal/my_pthread.h"
#include "drizzled/sql/result_set.h"
#include <string>
#include <vector>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <google/protobuf/text_format.h>

using namespace std;
using namespace drizzled;

namespace slave
{

void QueueManager::processQueue(void)
{
  boost::posix_time::seconds duration(getCheckInterval());

  /* TODO: This is only needed b/c of plugin timing issues */
  try
  {
    boost::this_thread::sleep(duration);
  }
  catch (boost::thread_interrupted &)
  {
    return;
  }

  /* thread setup needed to do things like create a Session */
  internal::my_thread_init();
  boost::this_thread::at_thread_exit(&internal::my_thread_end);

  /* setup a Session object */
  _session= Session::make_shared(plugin::Listen::getNullClient(),
                                 catalog::local());
  identifier::User::shared_ptr user= identifier::User::make_shared();
  user->setUser("slave");
  _session->setUser(user);
  _session->set_db("replication");

  if (not createApplierSchemaAndTables())
    return;

  uint64_t trx_id= 0;

  while (1)
  {
    /* This uninterruptable block processes the message queue */
    {
      boost::this_thread::disable_interruption di;

      TrxIdList completedTransactionIds;

      getListOfCompletedTransactions(completedTransactionIds);

      for (size_t x= 0; x < completedTransactionIds.size(); x++)
      {
        string commit_id;
        trx_id= completedTransactionIds[x];

        vector<string> aggregate_sql;  /* final SQL to execute */
        vector<string> segmented_sql;  /* carryover from segmented statements */

        message::Transaction transaction;
        uint32_t segment_id= 1;

        while (getMessage(transaction, commit_id, trx_id, segment_id++))
        {
          convertToSQL(transaction, aggregate_sql, segmented_sql);
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
          if (not executeSQL(aggregate_sql, commit_id))
          {
            /* TODO: Handle errors better. For now, just shutdown the slave. */
            return;
          }
        }

        deleteFromQueue(trx_id);      
      }
    }
    
    /* Interruptable only when not doing work (aka, sleeping) */
    try
    {
      boost::this_thread::sleep(duration);
    }
    catch (boost::thread_interrupted &)
    {
      return;
    }
  }
}


bool QueueManager::createApplierSchemaAndTables()
{
  vector<string> sql;
 
  sql.push_back("COMMIT");
  sql.push_back("CREATE SCHEMA IF NOT EXISTS replication");
  
  if (not executeSQL(sql))
    return false;
  
  /*
   * Create our applier thread state information table if we need to.
   */

  sql.clear();
  sql.push_back("COMMIT");
  sql.push_back("CREATE TABLE IF NOT EXISTS replication.applier_state"
                " (last_applied_commit_id BIGINT NOT NULL PRIMARY KEY,"
                " status VARCHAR(20) NOT NULL,"
                " error_msg VARCHAR(250))");
  
  if (not executeSQL(sql))
    return false;

  sql.clear();
  sql.push_back("SELECT COUNT(*) FROM replication.applier_state");
  
  sql::ResultSet result_set(1);
  Execute execute(*(_session.get()), true);
  execute.run(sql[0], result_set);
  result_set.next();
  string count= result_set.getString(0);

  /* Must always be at least one row in the table */
  if (count == "0")
  {
    sql.clear();
    sql.push_back("INSERT INTO replication.applier_state"
                  " (last_applied_commit_id, status)"
                  " VALUES (0, 'RUNNING')");
    if (not executeSQL(sql))
      return false;
  }
  else
  {
    setApplierState("", true);
  }

  /*
   * Create our message queue table if we need to.
   */

  sql.clear();
  sql.push_back("COMMIT");
  sql.push_back("CREATE TABLE IF NOT EXISTS replication.applier_queue"
                " (trx_id BIGINT NOT NULL, seg_id INT NOT NULL,"
                " commit_order INT, msg BLOB, PRIMARY KEY(trx_id, seg_id))");

  if (not executeSQL(sql))
    return false;
                
  return true;
}  
  
  
bool QueueManager::getMessage(message::Transaction &transaction,
                              string &commit_id,
                              uint64_t trx_id,
                              uint32_t segment_id)
{
  string sql("SELECT msg, commit_order FROM replication.applier_queue"
             " WHERE trx_id = ");
  sql.append(boost::lexical_cast<std::string>(trx_id));
  sql.append(" AND seg_id = ");
  sql.append(boost::lexical_cast<std::string>(segment_id));

  sql::ResultSet result_set(2);
  Execute execute(*(_session.get()), true);
  
  execute.run(sql, result_set);
  
  assert(result_set.getMetaData().getColumnCount() == 2);

  /* Really should only be 1 returned row */
  uint32_t found_rows= 0;
  while (result_set.next())
  {
    string msg= result_set.getString(0);
    string com_id= result_set.getString(1);

    if ((msg == "") || (found_rows == 1))
      break;

    /* Neither column should be NULL */
    assert(result_set.isNull(0) == false);
    assert(result_set.isNull(1) == false);

    //transaction.ParseFromString(value);
    google::protobuf::TextFormat::ParseFromString(msg, &transaction);
    commit_id= com_id;

    found_rows++;
  }

  if (found_rows == 0)
    return false;
  
  return true;
}

bool QueueManager::getListOfCompletedTransactions(TrxIdList &list)
{
  Execute execute(*(_session.get()), true);
  
  string sql("SELECT trx_id FROM replication.applier_queue"
             " WHERE commit_order IS NOT NULL ORDER BY commit_order ASC");
  
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


bool QueueManager::convertToSQL(const message::Transaction &transaction,
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

    /* Replace any embedded NULLs in the SQL */
    vector<string>::iterator iter;
    for (iter= segmented_sql.begin(); iter != segmented_sql.end(); ++iter)
    {
      string &sql= *iter;
      string::size_type found= sql.find_first_of('\0');
      while (found != string::npos)
      {
        sql[found]= '\\';
        sql.insert(found + 1, 1, '0');
        found= sql.find_first_of('\0', found);
      }      
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


bool QueueManager::isEndStatement(const message::Statement &statement)
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


void QueueManager::setApplierState(const string &err_msg, bool status)
{
  vector<string> statements;
  string sql;
  string msg(err_msg);

  if (not status)
  {
    sql= "UPDATE replication.applier_state SET status = 'STOPPED'";
  }
  else
  {
    sql= "UPDATE replication.applier_state SET status = 'RUNNING'";
  }
  
  sql.append(", error_msg = '");

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
  sql.append("'");

  statements.push_back(sql);
  executeSQL(statements);
}


bool QueueManager::executeSQL(vector<string> &sql,
                              const string &commit_id)
{
  string tmp("UPDATE replication.applier_state"
             " SET last_applied_commit_id = ");
  tmp.append(commit_id);
  sql.push_back(tmp);
  
  return executeSQL(sql);
}


bool QueueManager::executeSQL(vector<string> &sql)
{
  string combined_sql;

  Execute execute(*(_session.get()), true);

  vector<string>::iterator iter= sql.begin();

  while (iter != sql.end())
  {
    combined_sql.append(*iter);
    combined_sql.append("; ");
    ++iter;
  }

  //printf("execute: %s\n", combined_sql.c_str()); fflush(stdout);

  sql::ResultSet result_set(1);

  /* Execute wraps the SQL to run within a transaction */
  execute.run(combined_sql, result_set);

  sql::Exception exception= result_set.getException();
  
  drizzled::error_t err= exception.getErrorCode();

  if ((err != drizzled::EE_OK) && (err != drizzled::ER_EMPTY_QUERY))
  {
    /* avoid recursive errors from setApplierState() */
    if (_in_error_state)
      return true;

    _in_error_state= true;
    string err_msg("(SQLSTATE ");
    err_msg.append(exception.getSQLState());
    err_msg.append(") ");
    err_msg.append(exception.getErrorMessage());

    std::cerr << err_msg << std::endl;
    std::cerr << "Slave failed while executing:\n";
    for (size_t y= 0; y < sql.size(); y++)
      std::cerr << sql[y] << std::endl;

    setApplierState(err_msg, false);
    return false;
  }

  return true;
}


bool QueueManager::deleteFromQueue(uint64_t trx_id)
{
  string sql("DELETE FROM replication.applier_queue WHERE trx_id = ");
  sql.append(boost::lexical_cast<std::string>(trx_id));

  vector<string> sql_vect;
  sql_vect.push_back(sql);

  return executeSQL(sql_vect);
}

} /* namespace slave */
