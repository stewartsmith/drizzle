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

DRIZZLE_PLUGIN(syslog_module::init, syslog_module::system_variables, NULL);
