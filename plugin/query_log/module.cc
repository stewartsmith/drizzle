/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright 2011 Daniel Nichter
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <string>
#include <boost/program_options.hpp>
#include <drizzled/item.h>
#include <drizzled/data_home.h>
#include <drizzled/module/option_map.h>
#include <drizzled/session.h>
#include <drizzled/plugin.h>
#include "query_log.h"

namespace po= boost::program_options;
using namespace std;
using namespace drizzled;

namespace drizzle_plugin {

/**
 * Forward declarations.
 * Mac OS X 10.6 with gcc 4.2.1 misses this warning (but still compiles):
 *   configure:23893: g++ -c -Werror -pedantic -Wmissing-declarations \
 *     -std=gnu++98  -O0 -DDEBUG   conftest.cpp >&5
 *   cc1plus: warnings being treated as errors
 *   cc1plus: warning: command line option "-Wmissing-declarations" is valid
 *     for C/ObjC but not for C++
 */
bool update_file(Session *, set_var *var);
void update_file_enabled(Session *, sql_var_t);

/**
 * An instance of our class, QueryLog, which implements the query_log plugin,
 * and an instance of the QueryLoggerClass for logging queries to a file.
 * These objects are global so update_file() and update_file_enabled() can
 * access them.  They're instantiated in init_options().
 */
static drizzled::plugin::QueryLog *query_log= NULL;
static QueryLoggerFile *logger_file= NULL;

/**
 * Sensible defaults.
 */
const char *default_file= "drizzled-queries.log"; ///< Default query log file

/**
 * @brief
 *   Update query_log_file (query_log->sysvar_file).
 *
 * @details
 *   When SET GLOBAL query_log_file="new-file.log" is executed by the user,
 *   this function is called which checkes that the new file is not NULL or
 *   a blank string, then calls logger_file->openLogFile, passing it the new
 *   log file name, e.g. "new-file.log".  If the new log file is opened, the
 *   system variable is updated, else it is not updated and logging continues
 *   to the old log file.
 *
 * @retval true  Error, log file not changed
 * @retval false Success, log file changed
 */  
bool update_file(Session *, set_var *var)
{
  const char *new_file= var->value->str_value.ptr();

  if (not new_file)
  {
    errmsg_printf(error::ERROR, _("The query log file name must be defined."));
    return false;
  }

  if (not *new_file)
  {
    errmsg_printf(error::ERROR, _("The query log file name must have a value."));
    return false;
  }

  /**
   * If the the log file is enabled, then try to open the new log file.
   * If it's not enabled, then just update query_log_file because the user
   * might be trying to:
   *   close current log file (SET GLOBAL log_file_enabled=FALSE)
   *   switch to new log file (SET GLOBAL log_file="new-file.log")
   *   enable new log file (SET GLOBAL log_file_enabled=TRUE)
   * (Maybe they're doing this to rotate the log?)  If this is the case,
   * then we don't want to open the new log file before it's enabled,
   * but we also don't want to leave query_log_file set to the old log
   * file name.  When the log file is re-enabled later, update_file_enabled()
   * will be called and the new log file will be opened.
   */
  if (query_log->sysvar_file_enabled)
  {
    if (logger_file->openLogFile(new_file))
    {
      errmsg_printf(error::ERROR, "Cannot open the query log file %s", new_file);
      return true; // error
    }
  }

  // Update query_log_file in SHOW VARIABLES.
  query_log->sysvar_file= new_file;

  return false;  // success
}

/**
 * @brief
 *   Update query_log_file_enabled (query_log->sysvar_file_enabled).
 *
 * @details
 *   When SET GLOBAL query_log_file_enabled=... is executed by the user,
 *   this function is called *after* Drizzle updates the variable with the
 *   new value, so in this function we have the new/current value.  If the
 *   log file is enabled, then open query_log_file (query_log->sysvar_file);
 *   else, close the log file.
 */  
void update_file_enabled(Session *, sql_var_t)
{
  if (query_log->sysvar_file_enabled)
  {
    if (logger_file->openLogFile(query_log->sysvar_file.c_str()))
    {
      errmsg_printf(error::ERROR, "Cannot enable the query log file because the query log file %s cannot be opened.", query_log->sysvar_file.c_str());
      query_log->sysvar_file_enabled= false;
    }
  }
  else
    logger_file->closeLogFile();
}

/**
 * @brief
 *   Initialize query-log command line options.
 *
 * @details
 *   This function is called first, before init().  We instantiate our one
 *   and only QueryLog object (query_log) here so that po (boost::program_options)
 *   can store the command line options' values in public query_log variables.
 *   This avoids using global variables and keeps (almost) everything encapsulated
 *   in query_log.
 */
static void init_options(drizzled::module::option_context &context)
{
  logger_file= new QueryLoggerFile();
  query_log= new drizzled::plugin::QueryLog(true, logger_file);

  context(
    "file-enabled",
    po::value<bool>(&query_log->sysvar_file_enabled)->default_value(false)->zero_tokens(),
    N_("Enable query logging to file"));

  context(
    "file",
    po::value<string>(&query_log->sysvar_file)->default_value(default_file),
    N_("Query log file"));

  context(
    "threshold-execution-time",
    po::value<uint32_constraint>(&query_log->sysvar_threshold_execution_time)->default_value(0),
    _("Threshold for logging slow queries, in microseconds"));
  
  context(
    "threshold-lock-time",
    po::value<uint32_constraint>(&query_log->sysvar_threshold_lock_time)->default_value(0),
    _("Threshold for logging long locking queries, in microseconds"));
  
  context(
    "threshold-rows-examined",
    po::value<uint32_constraint>(&query_log->sysvar_threshold_rows_examined)->default_value(0),
    _("Threshold for logging queries that examine too many rows, integer"));
  
  context(
    "threshold-rows-sent",
    po::value<uint32_constraint>(&query_log->sysvar_threshold_rows_sent)->default_value(0),
    _("Threshold for logging queries that return too many rows, integer"));
  
  context(
    "threshold-tmp-tables",
    po::value<uint32_constraint>(&query_log->sysvar_threshold_tmp_tables)->default_value(0),
    _("Threshold for logging queries that use too many temporary tables, integer"));

  context(
    "threshold-warnings",
    po::value<uint32_constraint>(&query_log->sysvar_threshold_warnings)->default_value(0),
    _("Threshold for logging queries that cause too many warnings, integer"));

  context(
    "threshold-session-time",
    po::value<uint32_constraint>(&query_log->sysvar_threshold_session_time)->default_value(0),
    _("Threshold for logging queries that are active too long, in seconds"));

}

/**
 * @brief
 *   Add query_log plugin to Drizzle and initalize query_log system variables.
 *
 * @details
 *   This is where we plug into Drizzle and register our system variables.
 *   Since this is called after init_options(), vm has either values from
 *   the command line or defaults.  System variables corresponding to
 *   command line options use the same public query_log variables so that
 *   values from vm (the command line) are automatically reflected in the
 *   system variable (SHOW VARIABLES).  This also makes changes to certain
 *   system variables automatic/instant because when they're updated (e.g.
 *   SET GLOBAL query_log_enabled=TRUE|FALSE) Drizzle changes the corresponding
 *   public query_log variable.  Certain system variables, like query_log_file,
 *   require more work to change, so they're handled by update functions like
 *   update_file().
 *
 * @retval 0 Success
 */
static int init(drizzled::module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  // Open the log file now that we have either an explicit value from the
  // command line (--query-log.file=FILE) or the default file.
  if (vm["file-enabled"].as<bool>())
    logger_file->openLogFile(vm["file"].as<string>().c_str());

  // Plug into Drizzle!
  context.add(query_log);

  // Register our system variables with Drizzle, e.g. query_log_enabled,
  // query_log_file, etc. in SHOW VARIABLES.
  context.registerVariable(
    new sys_var_bool_ptr(
      "enabled", &query_log->sysvar_enabled));

  context.registerVariable(
    new sys_var_bool_ptr(
      "file_enabled", &query_log->sysvar_file_enabled, &update_file_enabled));

  context.registerVariable(
    new sys_var_std_string(
      "file", query_log->sysvar_file, NULL, &update_file));

  context.registerVariable(
    new sys_var_constrained_value<uint32_t>(
      "threshold_execution_time", query_log->sysvar_threshold_execution_time));
  
  context.registerVariable(
    new sys_var_constrained_value<uint32_t>(
      "threshold_lock_time", query_log->sysvar_threshold_lock_time));
  
  context.registerVariable(
    new sys_var_constrained_value<uint32_t>(
      "threshold_rows_examined", query_log->sysvar_threshold_rows_examined));
  
  context.registerVariable(
    new sys_var_constrained_value<uint32_t>(
      "threshold_rows_sent", query_log->sysvar_threshold_rows_sent));
  
  context.registerVariable(
    new sys_var_constrained_value<uint32_t>(
      "threshold_tmp_tables", query_log->sysvar_threshold_tmp_tables));
  
  context.registerVariable(
    new sys_var_constrained_value<uint32_t>(
      "threshold_warnings", query_log->sysvar_threshold_warnings));

  context.registerVariable(
    new sys_var_constrained_value<uint32_t>(
      "threshold_session_time", query_log->sysvar_threshold_session_time));

  return 0; // success
}

} /* namespace drizzle_plugin */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "query_log",
  "1.0",
  "Daniel Nichter",
  N_("Logs queries to a file"),
  PLUGIN_LICENSE_GPL,
  drizzle_plugin::init,
  NULL,
  drizzle_plugin::init_options
}
DRIZZLE_DECLARE_PLUGIN_END;
