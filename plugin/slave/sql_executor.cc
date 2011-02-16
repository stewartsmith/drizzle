#include "config.h"
#include "plugin/slave/sql_executor.h"
#include <drizzled/plugin/listen.h>
#include <drizzled/plugin/client.h>
#include <drizzled/catalog/local.h>
#include <drizzled/execute.h>
#include <drizzled/sql/result_set.h>
#include <drizzled/errmsg_print.h>

using namespace std;
using namespace drizzled;

namespace slave
{

SQLExecutor::SQLExecutor(const string &user, const string &schema)
  : _in_error_state(false)
{
  /* setup a Session object */
  _session= Session::make_shared(plugin::Listen::getNullClient(),
                                 catalog::local());
  identifier::User::shared_ptr user_id= identifier::User::make_shared();
  user_id->setUser(user);
  _session->setUser(user_id);
  _session->set_db(schema);
}


bool SQLExecutor::executeSQL(vector<string> &sql)
{
  string combined_sql;

  if (not _in_error_state)
    _error_message.clear();

  Execute execute(*(_session.get()), true);

  vector<string>::iterator iter= sql.begin();

  while (iter != sql.end())
  {
    combined_sql.append(*iter);
    combined_sql.append("; ");
    ++iter;
  }

  sql::ResultSet result_set(1);

  /* Execute wraps the SQL to run within a transaction */
  execute.run(combined_sql, result_set);

  sql::Exception exception= result_set.getException();

  drizzled::error_t err= exception.getErrorCode();

  if ((err != drizzled::EE_OK) && (err != drizzled::ER_EMPTY_QUERY))
  {
    /* avoid recursive errors */
    if (_in_error_state)
      return true;

    _in_error_state= true;
    _error_message= "(SQLSTATE ";
    _error_message.append(exception.getSQLState());
    _error_message.append(") ");
    _error_message.append(exception.getErrorMessage());

    string bad_sql("Failure while executing:\n");
    for (size_t y= 0; y < sql.size(); y++)
    {
      bad_sql.append(sql[y]);
      bad_sql.append("\n");
    }

    errmsg_printf(error::ERROR, _("%s\n%s\n"),
                  _error_message.c_str(), bad_sql.c_str());
    return false;
  }

  return true;
}

} /* namespace slave */
