/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Mark Atwood
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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
#include "module.h"

#include <drizzled/plugin.h>
#include <drizzled/plugin/logging.h>
#include <drizzled/plugin/error_message.h>
#include <drizzled/plugin/function.h>

#include "logging.h"
#include "errmsg.h"
#include "function.h"
#include <iostream>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>

namespace po= boost::program_options;
using namespace std;
using namespace drizzled;

namespace syslog_module
{

char* sysvar_ident;
char* sysvar_facility;
bool sysvar_logging_enable;
char* sysvar_logging_priority;
unsigned long sysvar_logging_threshold_slow;
unsigned long sysvar_logging_threshold_big_resultset;
unsigned long sysvar_logging_threshold_big_examined;
bool sysvar_errmsg_enable;
char* sysvar_errmsg_priority;


static int init(drizzled::module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  if (vm.count("logging_threshold-slow"))
  {
    if (sysvar_logging_threshold_slow > ULONG_MAX)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value for logging-threshold-slow\n"));
      return 1;
    }
  }

  if (vm.count("logging-threshold-big-resultset"))
  {
    if (sysvar_logging_threshold_big_resultset > 2)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value for logging-threshold-big-resultset\n"));
      return 1;
    }
  }

  if (vm.count("logging-threshold-big-examined"))
  {
    if (sysvar_logging_threshold_big_examined > 2)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value for logging-threshold-big-examined\n"));
      return 1;
    }
  }

  if (vm.count("ident"))
  {
    sysvar_ident= const_cast<char *>(vm["ident"].as<string>().c_str());
  }
  else
  {
    sysvar_ident= const_cast<char *>("drizzled");
  }

  if (vm.count("facility"))
  {
    sysvar_facility= const_cast<char *>(vm["facility"].as<string>().c_str());
  }
  else
  {
    sysvar_facility= const_cast<char *>("local0");
  }

  if (vm.count("logging-priority"))
  {
    sysvar_logging_priority= const_cast<char *>(vm["logging-priority"].as<string>().c_str());
  }
  else
  {
    sysvar_logging_priority= const_cast<char *>("info");
  }

  if (vm.count("errmsg-priority"))
  {
    sysvar_errmsg_priority= const_cast<char *>(vm["errmsg-priority"].as<string>().c_str());
  }

  else
  {
    sysvar_errmsg_priority= const_cast<char *>("warning");
  }

  context.add(new Logging_syslog());
  context.add(new ErrorMessage_syslog());
  context.add(new plugin::Create_function<Function_syslog>("syslog"));
  return 0;
}

static DRIZZLE_SYSVAR_STR(
  ident,
  sysvar_ident,
  PLUGIN_VAR_READONLY,
  N_("Syslog Ident"),
  NULL, /* check func */
  NULL, /* update func*/
  "drizzled" /* default */);

static DRIZZLE_SYSVAR_STR(
  facility,
  sysvar_facility,
  PLUGIN_VAR_READONLY,
  N_("Syslog Facility"),
  NULL, /* check func */
  NULL, /* update func*/
  "local0" /* default */);  // local0 is what PostGreSQL uses by default

static DRIZZLE_SYSVAR_BOOL(
  logging_enable,
  sysvar_logging_enable,
  PLUGIN_VAR_NOCMDARG,
  N_("Enable logging to syslog of the query log"),
  NULL, /* check func */
  NULL, /* update func */
  false /* default */);

static DRIZZLE_SYSVAR_STR(
  logging_priority,
  sysvar_logging_priority,
  PLUGIN_VAR_READONLY,
  N_("Syslog Priority of query logging"),
  NULL, /* check func */
  NULL, /* update func*/
  "info" /* default */);

static DRIZZLE_SYSVAR_ULONG(
  logging_threshold_slow,
  sysvar_logging_threshold_slow,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging slow queries, in microseconds"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  ULONG_MAX, /* max */
  0 /* blksiz */);

static DRIZZLE_SYSVAR_ULONG(
  logging_threshold_big_resultset,
  sysvar_logging_threshold_big_resultset,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging big queries, for rows returned"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  ULONG_MAX, /* max */
  0 /* blksiz */);

static DRIZZLE_SYSVAR_ULONG(
  logging_threshold_big_examined,
  sysvar_logging_threshold_big_examined,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging big queries, for rows examined"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  ULONG_MAX, /* max */
  0 /* blksiz */);

static DRIZZLE_SYSVAR_BOOL(
  errmsg_enable,
  sysvar_errmsg_enable,
  PLUGIN_VAR_NOCMDARG,
  N_("Enable logging to syslog of the error messages"),
  NULL, /* check func */
  NULL, /* update func */
  false /* default */);

static DRIZZLE_SYSVAR_STR(
  errmsg_priority,
  sysvar_errmsg_priority,
  PLUGIN_VAR_READONLY,
  N_("Syslog Priority of error messages"),
  NULL, /* check func */
  NULL, /* update func*/
  "warning" /* default */);

static void init_options(drizzled::module::option_context &context)
{
  context("ident",
          po::value<string>(),
          N_("Syslog Ident"));
  context("facility",
          po::value<string>(),
          N_("Syslog Facility"));
  context("logging-enable",
          po::value<bool>(&sysvar_logging_enable)->default_value(false)->zero_tokens(),
          N_("Enable logging to syslog of the query log"));
  context("logging-priority",
          po::value<string>(),
          N_("Syslog Priority of query logging"));
  context("logging-threshold-slow",
          po::value<unsigned long>(&sysvar_logging_threshold_slow)->default_value(0),
          N_("Threshold for logging slow queries, in microseconds"));
  context("logging-threshold-big-resultset",
          po::value<unsigned long>(&sysvar_logging_threshold_big_resultset)->default_value(0),
          N_("Threshold for logging big queries, for rows returned"));
  context("logging-threshold-big-examined",
          po::value<unsigned long>(&sysvar_logging_threshold_big_examined)->default_value(0),
          N_("Threshold for logging big queries, for rows examined"));
  context("errmsg-enable",
          po::value<bool>(&sysvar_errmsg_enable)->default_value(false)->zero_tokens(),
          N_("Enable logging to syslog of the error messages"));
  context("errmsg-priority",
          po::value<string>(),
          N_("Syslog Priority of error messages"));
}

static drizzle_sys_var* system_variables[]= {
  DRIZZLE_SYSVAR(ident),
  DRIZZLE_SYSVAR(facility),
  DRIZZLE_SYSVAR(logging_enable),
  DRIZZLE_SYSVAR(logging_priority),
  DRIZZLE_SYSVAR(logging_threshold_slow),
  DRIZZLE_SYSVAR(logging_threshold_big_resultset),
  DRIZZLE_SYSVAR(logging_threshold_big_examined),
  DRIZZLE_SYSVAR(errmsg_enable),
  DRIZZLE_SYSVAR(errmsg_priority),
  NULL
};

} // namespace syslog_module

DRIZZLE_PLUGIN(syslog_module::init, syslog_module::system_variables, syslog_module::init_options);
