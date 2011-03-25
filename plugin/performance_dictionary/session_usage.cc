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

#include <plugin/performance_dictionary/dictionary.h>

#include <drizzled/atomics.h>
#include <drizzled/session.h>

#include <sys/resource.h>

using namespace drizzled;
using namespace std;

#define FUNCTION_NAME_LEN 64

performance_dictionary::SessionUsage::SessionUsage() :
  plugin::TableFunction("DATA_DICTIONARY", "SESSION_USAGE")
{
  add_field("QUERY", plugin::TableFunction::STRING, FUNCTION_NAME_LEN, false);
  add_field("USER_TIME_USED_SECONDS", plugin::TableFunction::NUMBER, false);
  add_field("USER_TIME_USED_MICRO_SECONDS", plugin::TableFunction::NUMBER, false);
  add_field("SYSTEM_TIME_USED_SECONDS", plugin::TableFunction::NUMBER, false);
  add_field("SYSTEM_TIME_USED_MICRO_SECONDS", plugin::TableFunction::NUMBER, false);
  add_field("INTEGRAL_MAX_RESIDENT_SET_SIZE", plugin::TableFunction::NUMBER, 0, false);
  add_field("INTEGRAL_SHARED_TEXT_MEMORY_SIZE", plugin::TableFunction::NUMBER, 0, false);
  add_field("INTEGRAL_UNSHARED_DATA_SIZE", plugin::TableFunction::NUMBER, 0, false);
  add_field("INTEGRAL_UNSHARED_STACK_SIZE", plugin::TableFunction::NUMBER, 0, false);
  add_field("PAGE_RECLAIMS", plugin::TableFunction::NUMBER, 0, false);
  add_field("PAGE_FAULTS", plugin::TableFunction::NUMBER, 0, false);
  add_field("SWAPS", plugin::TableFunction::NUMBER, 0, false);
  add_field("BLOCK_INPUT_OPERATIONS", plugin::TableFunction::NUMBER, 0, false);
  add_field("BLOCK_OUTPUT_OPERATIONS", plugin::TableFunction::NUMBER, 0, false);
  add_field("MESSAGES_SENT", plugin::TableFunction::NUMBER, 0, false);
  add_field("MESSAGES_RECEIVED", plugin::TableFunction::NUMBER, 0, false);
  add_field("SIGNALS_RECEIVED", plugin::TableFunction::NUMBER, 0, false);
  add_field("VOLUNTARY_CONTEXT_SWITCHES", plugin::TableFunction::NUMBER, 0, false);
  add_field("INVOLUNTARY_CONTEXT_SWITCHES", plugin::TableFunction::NUMBER, 0, false);
}


performance_dictionary::SessionUsage::Generator::Generator(drizzled::Field **arg) :
  drizzled::plugin::TableFunction::Generator(arg),
  usage_cache(0)
{ 
  usage_cache= getSession().getProperty<QueryUsage>("query_usage");
  if (usage_cache)
    query_iter= usage_cache->list().rbegin();
}

bool performance_dictionary::SessionUsage::Generator::populate()
{
  if (not usage_cache)
    return false;

  if (query_iter == usage_cache->list().rend())
    return false;

  publish(query_iter->query, query_iter->delta());
  query_iter++;

  return true;
}

void performance_dictionary::SessionUsage::Generator::publish(const std::string &sql, const struct rusage &usage_arg)
{
  /* SQL */
  push(sql.substr(0, FUNCTION_NAME_LEN));

  /* USER_TIME_USED_SECONDS */
  push(static_cast<int64_t>(usage_arg.ru_utime.tv_sec));

  /* USER_TIME_USED_MICRO_SECONDS */
  push(static_cast<int64_t>(usage_arg.ru_utime.tv_usec));

  /* SYSTEM_TIME_USED_SECONDS */
  push(static_cast<int64_t>(usage_arg.ru_stime.tv_sec));

  /* SYSTEM_TIME_USED_MICRO_SECONDS */
  push(static_cast<int64_t>(usage_arg.ru_stime.tv_usec));

  /* INTEGRAL_MAX_RESIDENT_SET_SIZE */
  push(static_cast<int64_t>(usage_arg.ru_maxrss));

  /* INTEGRAL_SHARED_TEXT_MEMORY_SIZE */
  push(static_cast<int64_t>(usage_arg.ru_ixrss));

  /* INTEGRAL_UNSHARED_DATA_SIZE */
  push(static_cast<int64_t>(usage_arg.ru_idrss));

  /* INTEGRAL_UNSHARED_STACK_SIZE */
  push(static_cast<int64_t>(usage_arg.ru_isrss));

  /* PAGE_RECLAIMS */
  push(static_cast<int64_t>(usage_arg.ru_minflt));

  /* PAGE_FAULTS */
  push(static_cast<int64_t>(usage_arg.ru_majflt));

  /* SWAPS */
  push(static_cast<int64_t>(usage_arg.ru_nswap));

  /* BLOCK_INPUT_OPERATIONS */
  push(static_cast<int64_t>(usage_arg.ru_inblock));

  /* BLOCK_OUTPUT_OPERATIONS */
  push(static_cast<int64_t>(usage_arg.ru_oublock));

  /* MESSAGES_SENT */
  push(static_cast<int64_t>(usage_arg.ru_msgsnd));

  /* MESSAGES_RECEIVED */
  push(static_cast<int64_t>(usage_arg.ru_msgrcv));

  /* SIGNALS_RECEIVED */
  push(static_cast<int64_t>(usage_arg.ru_nsignals));

  /* VOLUNTARY_CONTEXT_SWITCHES */
  push(static_cast<int64_t>(usage_arg.ru_nvcsw));

  /* INVOLUNTARY_CONTEXT_SWITCHES */
  push(static_cast<int64_t>(usage_arg.ru_nivcsw));
}
