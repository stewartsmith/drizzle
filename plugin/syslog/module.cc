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

#include <config.h>

#include <drizzled/plugin.h>
#include <drizzled/plugin/logging.h>
#include <drizzled/plugin/error_message.h>
#include <drizzled/plugin/function.h>

#include "wrap.h"
#include "logging.h"
#include "errmsg.h"
#include "function.h"
#include <iostream>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>

namespace po= boost::program_options;
using namespace std;
using namespace drizzled;

namespace drizzle_plugin
{

bool sysvar_logging_enable= false;
bool sysvar_errmsg_enable= false;

uint64_constraint sysvar_logging_threshold_slow;
uint64_constraint sysvar_logging_threshold_big_resultset;
uint64_constraint sysvar_logging_threshold_big_examined;


static int init(drizzled::module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  WrapSyslog::singleton().openlog(vm["ident"].as<string>());
  if (sysvar_errmsg_enable)
  {
    context.add(new error_message::Syslog(vm["facility"].as<string>(),
                                          vm["errmsg-priority"].as<string>()));
  }
  if (sysvar_logging_enable)
  {
    context.add(new logging::Syslog(vm["facility"].as<string>(),
                                    vm["logging-priority"].as<string>(),
                                    sysvar_logging_threshold_slow.get(),
                                    sysvar_logging_threshold_big_resultset.get(),
                                    sysvar_logging_threshold_big_examined.get()));
  }
  context.add(new plugin::Create_function<udf::Syslog>("syslog"));

  context.registerVariable(new sys_var_const_string_val("facility",
                                                        vm["facility"].as<string>()));
  context.registerVariable(new sys_var_const_string_val("errmsg_priority",
                                                        vm["errmsg-priority"].as<string>()));
  context.registerVariable(new sys_var_const_string_val("logging_priority",
                                                        vm["logging-priority"].as<string>()));
  context.registerVariable(new sys_var_bool_ptr_readonly("logging_enable",
                                                         &sysvar_logging_enable));
  context.registerVariable(new sys_var_bool_ptr_readonly("errmsg_enable",
                                                         &sysvar_errmsg_enable));
  context.registerVariable(new sys_var_constrained_value_readonly<uint64_t>("logging_threshold_slow",
                                                                            sysvar_logging_threshold_slow));
  context.registerVariable(new sys_var_constrained_value_readonly<uint64_t>("logging_threshold_big_resultset",
                                                                            sysvar_logging_threshold_big_resultset));
  context.registerVariable(new sys_var_constrained_value_readonly<uint64_t>("logging_threshold_big_examined",
                                                                            sysvar_logging_threshold_big_examined));

  return 0;
}


static void init_options(drizzled::module::option_context &context)
{
  context("ident",
          po::value<string>()->default_value("drizzled"),
          _("Syslog Ident"));
  context("facility",
          po::value<string>()->default_value("local0"),
          _("Syslog Facility"));
  context("logging-enable",
          po::value<bool>(&sysvar_logging_enable)->default_value(false)->zero_tokens(),
          _("Enable logging to syslog of the query log"));
  context("logging-priority",
          po::value<string>()->default_value("warning"),
          _("Syslog Priority of query logging"));
  context("logging-threshold-slow",
          po::value<uint64_constraint>(&sysvar_logging_threshold_slow)->default_value(0),
          _("Threshold for logging slow queries, in microseconds"));
  context("logging-threshold-big-resultset",
          po::value<uint64_constraint>(&sysvar_logging_threshold_big_resultset)->default_value(0),
          _("Threshold for logging big queries, for rows returned"));
  context("logging-threshold-big-examined",
          po::value<uint64_constraint>(&sysvar_logging_threshold_big_examined)->default_value(0),
          _("Threshold for logging big queries, for rows examined"));
  context("errmsg-enable",
          po::value<bool>(&sysvar_errmsg_enable)->default_value(false)->zero_tokens(),
          _("Enable logging to syslog of the error messages"));
  context("errmsg-priority",
          po::value<string>()->default_value("warning"),
          _("Syslog Priority of error messages"));
}

} /* namespace drizzle_plugin */

DRIZZLE_PLUGIN(drizzle_plugin::init, NULL, drizzle_plugin::init_options);
