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
  Session::shared_ptr session= Session::make_shared(plugin::Listen::getNullClient(),
                                                    catalog::local());
  identifier::User::shared_ptr user= identifier::User::make_shared();
  user->setUser("slave");
  session->setUser(user);
  session->set_db(getSchema());

  uint64_t trx_id= 0;

  while (1)
  {
    /* This uninterruptable block processes the message queue */
    {
      boost::this_thread::disable_interruption di;

      TrxIdList completedTransactionIds;

      getListOfCompletedTransactions(*(session.get()), completedTransactionIds);

      for (size_t x= 0; x < completedTransactionIds.size(); x++)
      {
        trx_id= completedTransactionIds[x];

        vector<string> aggregate_sql;  /* final SQL to execute */
        vector<string> segmented_sql;  /* carryover from segmented statements */

        message::Transaction transaction;
        uint32_t segment_id= 1;

        while (getMessage(*(session.get()), transaction, trx_id, segment_id++))
          convertToSQL(transaction, aggregate_sql, segmented_sql);

        assert(segmented_sql.empty());

        if (not aggregate_sql.empty())
          executeSQL(*(session.get()), aggregate_sql);

        deleteFromQueue(*(session.get()), trx_id);      
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


bool QueueManager::getMessage(Session &session,
                              message::Transaction &transaction,
                              uint64_t trx_id,
                              uint32_t segment_id)
{
  string sql("SELECT msg FROM ");
  sql.append(getSchema());
  sql.append(".");
  sql.append(getTable());
  sql.append(" WHERE trx_id = ");
  sql.append(boost::lexical_cast<std::string>(trx_id));
  sql.append(" AND seg_id = ");
  sql.append(boost::lexical_cast<std::string>(segment_id));

  printf("%s\n", sql.c_str()); fflush(stdout);
  sql::ResultSet result_set(1);
  Execute execute(session, true);
  
  execute.run(sql, &result_set);
  
  assert(result_set.getMetaData().getColumnCount() == 1);

  /* Really should only be 1 returned row */
  
  uint32_t found_rows= 0;

  while (result_set.next())
  {
    string value= result_set.getString(0);
    
    if ((value == "") || (found_rows == 1))
      break;

    assert(result_set.isNull(0) == false);
    transaction.ParseFromArray(value.c_str(), value.length());

    found_rows++;
  }

  if (found_rows == 0)
    return false;
  
  return true;
}

bool QueueManager::getListOfCompletedTransactions(Session &session,
                                                  TrxIdList &list)
{
  Execute execute(session, true);
  
  string sql("SELECT trx_id FROM ");
  sql.append(getSchema());
  sql.append(".");
  sql.append(getTable());
  sql.append(" WHERE commit_order IS NOT NULL ORDER BY commit_order ASC");
  
  /* ResultSet size must match column count */
  sql::ResultSet result_set(1);

  execute.run(sql, &result_set);

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

    if (statement.type() == message::Statement::ROLLBACK_STATEMENT)
    {
      segmented_sql.clear();
      continue;
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


bool QueueManager::executeSQL(Session &session, vector<string> &sql)
{
  string combined_sql;

  Execute execute(session, true);

  vector<string>::iterator iter= sql.begin();

  while (iter != sql.end())
  {
    combined_sql.append(*iter);
    combined_sql.append("; ");
    ++iter;
  }

  printf("execute: %s\n", combined_sql.c_str()); fflush(stdout);

  /* Execute wraps the SQL to run within a transaction */
  execute.run(combined_sql);

  return true;
}


bool QueueManager::deleteFromQueue(Session &session, uint64_t trx_id)
{
  string sql("DELETE FROM ");
  sql.append(getSchema());
  sql.append(".");
  sql.append(getTable());
  sql.append(" WHERE trx_id = ");
  sql.append(boost::lexical_cast<std::string>(trx_id));

  vector<string> sql_vect;
  sql_vect.push_back(sql);

  return executeSQL(session, sql_vect);
}

} /* namespace slave */
