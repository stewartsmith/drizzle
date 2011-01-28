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
  
  /* thread setup needed to do things like create a Session */
  internal::my_thread_init();
  boost::this_thread::at_thread_exit(&internal::my_thread_end);

  Session::shared_ptr session;

  if (session= Session::make_shared(plugin::Listen::getNullClient(), catalog::local()))
  {
    currentSession().release();
    currentSession().reset(session.get());
  }
  else
  {
    printf("Slave thread could not create a Session\n"); fflush(stdout);
    return;
  }

  uint64_t trx_id= 0;
  while (1)
  {
    /* This uninterruptable block processes the message queue */
    {
      boost::this_thread::disable_interruption di;

      while (findCompleteTransaction(&trx_id))
      {
        message::Transaction transaction;
        //while (getMessage(transaction, trx_id))
        {
          executeMessage(*(session.get()), transaction);
        }
      }

      deleteFromQueue(*(session.get()), trx_id);      

      trx_id++;
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


bool QueueManager::findCompleteTransaction(uint64_t *trx_id)
{
  (void)trx_id;
  /* search the queue, in insert order, for the first completed transaction */
  /* set trx_id to the transaction id found */
  return false;
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

  
bool QueueManager::executeMessage(Session &session,
                                  const message::Transaction &transaction)
{
  if (transaction.has_event())
    return true;

  /* SQL strings corresponding to this Statement */
  vector<string> statement_sql;

  statement_sql.push_back("START TRANSACTION");

  size_t num_statements= transaction.statement_size();

  for (size_t idx= 0; idx < num_statements; idx++)
  {
    enum message::TransformSqlError err;
    const message::Statement &statement= transaction.statement(idx);
    vector<string> temp_sql;
    vector<string>::iterator iter;

    err= message::transformStatementToSql(statement,
                                          temp_sql,
                                          message::DRIZZLE,
                                          true);

    /* Replace any embedded NULLs in the SQL */
    for (iter= temp_sql.begin(); iter != temp_sql.end(); ++iter)
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

    statement_sql.insert(statement_sql.end(),
                         temp_sql.begin(),
                         temp_sql.end());

    /*
     * We won't execute what is in our SQL cache until we have a complete
     * statement. This is so we can easily dump the changes if we get a
     * statement rollaback request.
     */
    if (isEndStatement(statement))
    {
      if (statement.type() != message::Statement::ROLLBACK_STATEMENT)
      {
        executeSQL(session, statement_sql);
      }
      statement_sql.clear();
    }    
  }

  /*
   * A ROLLBACK is added for us if needed, but we need to manually do the
   * COMMIT.
   */
  assert(statement_sql.empty());
  statement_sql.push_back("COMMIT");
  executeSQL(session, statement_sql);

  return true;
}


bool QueueManager::executeSQL(Session &session, vector<string> &sql)
{
  Execute execute(session, true);
  
  vector<string>::iterator iter= sql.begin();
  
  while (iter != sql.end())
  {
    printf("execute: %s\n", iter->c_str()); fflush(stdout);
    execute.run(*iter);
    ++iter;
  }
  
  return true;
}


bool QueueManager::deleteFromQueue(Session &session, uint64_t trx_id)
{
  string sql("DELETE FROM ");
  sql.append(getSchema());
  sql.append(".");
  sql.append(getTable());
  sql.append(" WHERE id = ");
  sql.append(boost::lexical_cast<std::string>(trx_id));
  
  vector<string> sql_vect;
  sql_vect.push_back(sql);

  return executeSQL(session, sql_vect);
}

} /* namespace slave */