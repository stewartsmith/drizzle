/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
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

#include <drizzled/statement/execute.h>
#include <drizzled/session.h>
#include <drizzled/execute.h>
#include <drizzled/user_var_entry.h>
#include <drizzled/plugin/listen.h>
#include <drizzled/plugin/client.h>
#include <drizzled/plugin/null_client.h>
#include <drizzled/plugin/client/concurrent.h>
#include <drizzled/sql_lex.h>

namespace drizzled {

void parse(drizzled::Session&, const char *inBuf, uint32_t length);

namespace statement {

Execute::Execute(Session *in_session,
                 drizzled::execute_string_t to_execute_arg,
                 bool is_quiet_arg,
                 bool is_concurrent_arg,
                 bool should_wait_arg) :
  Statement(in_session),
  is_quiet(is_quiet_arg),
  is_concurrent(is_concurrent_arg),
  should_wait(should_wait_arg),
  to_execute(to_execute_arg)
{
}
  

bool statement::Execute::parseVariable()
{
  if (to_execute.isVariable())
  {
    user_var_entry *var= session().getVariable(to_execute, false);

    if (var && var->length && var->value && var->type == STRING_RESULT)
    {
      LEX_STRING tmp_for_var;
      tmp_for_var.str= var->value; 
      tmp_for_var.length= var->length; 
      to_execute.set(tmp_for_var);

      return true;
    }
  }

  return false;
}


bool statement::Execute::runStatement(plugin::NullClient *client, const std::string &arg)
{
  client->pushSQL(arg);
  if (not session().executeStatement())
    return true;

  if (session().is_error())
    return true;

  return false;
}


bool statement::Execute::execute()
{
  bool ret= execute_shell();

  // We have to restore ourselves at the top for delete() to work.
  lex().statement= this;

  return ret;
}


bool statement::Execute::execute_shell()
{
  if (to_execute.length == 0)
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "Invalid Variable");
    return false;
  }

  if (to_execute.isVariable())
  {
    if (not parseVariable())
    {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "Invalid Variable");
      return false;
    }
  }

  if (is_concurrent)
  {
    if (not session().isConcurrentExecuteAllowed())
    {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "A Concurrent Execution Session can not launch another session.");
      return false;
    }

    drizzled::Execute executer(session(), should_wait);
    executer.run(to_execute.str, to_execute.length);
  }
  else // Non-concurrent run.
  {
    if (is_quiet)
    {
      plugin::Client *temp= session().getClient();
      plugin::NullClient *null_client= new plugin::NullClient;

      session().setClient(null_client);
      
      bool error_occured= false;
      bool is_savepoint= false;
      {
        std::string start_sql;
        if (session().inTransaction())
        {
          // @todo Figure out something a bit more solid then this.
          start_sql.append("SAVEPOINT execute_internal_savepoint");
          is_savepoint= true;
        }
        else
        {
          start_sql.append("START TRANSACTION");
        }

        error_occured= runStatement(null_client, start_sql);
      }

      // @note this is copied from code in NULL client, all of this belongs
      // in the pluggable parser pieces.  
      if (not error_occured)
      {
        typedef boost::tokenizer<boost::escaped_list_separator<char> > Tokenizer;
        std::string full_string(to_execute.str, to_execute.length);
        Tokenizer tok(full_string, boost::escaped_list_separator<char>("\\", ";", "\""));

        for (Tokenizer::iterator iter= tok.begin();
             iter != tok.end() and session().getKilled() != Session::KILL_CONNECTION;
             ++iter)
        {
          if (runStatement(null_client, *iter))
          {
            error_occured= true;
            break;
          }
        }

        // @todo Encapsulate logic later to method
        {
          std::string final_sql;
          if (is_savepoint)
          {
            final_sql.append(error_occured ? 
              "ROLLBACK TO SAVEPOINT execute_internal_savepoint" : 
              "RELEASE SAVEPOINT execute_internal_savepoint");
          }
          else
          {
            final_sql.append(error_occured ? "ROLLBACK" : "COMMIT");
          }

          // Run the cleanup command, we currently ignore if an error occurs
          // here.
          (void)runStatement(null_client, final_sql);
        }
      }

      session().setClient(temp);
      if (session().is_error())
      {
        session().clear_error(true);
      }
      else
      {
        session().clearDiagnostics();
      }

      session().my_ok();

      null_client->close();
      delete null_client;
    }
    else
    {
      parse(session(), to_execute.str, to_execute.length);
    }
  }

  return true;
}

} /* namespace statement */
} /* namespace drizzled */

