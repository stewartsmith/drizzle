/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor <mordred@inaugust.com>
 *  Copyright (C) 2011 Canonical, Ltd.
 *  Author: Clint Byrum <clint.byrum@canonical.com>
 *
 *  Copied from simple_user_policy
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

#include <drizzled/plugin/authorization.h>
#include <drizzled/module/option_map.h>

#include "policy.h"

namespace po= boost::program_options;

using namespace std;
using namespace drizzled;

namespace regex_policy
{

static int init(module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  Policy *policy= new Policy(fs::path(vm["policy"].as<string>()));
  if (not policy->loadFile())
  {
    errmsg_printf(error::ERROR, _("Could not load regex policy file: %s\n"),
                  (policy ? policy->getError().str().c_str() : _("Unknown")));
    delete policy;
    return 1;
  }

  context.add(policy);
  context.registerVariable(new sys_var_const_string_val("policy", vm["policy"].as<string>()));

  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("policy",
      po::value<string>()->default_value(DEFAULT_POLICY_FILE.string()),
      N_("File to load for regex authorization policies"));
}

bool Policy::loadFile()
{
  ifstream file(policy_file.string().c_str());
  boost::regex comment_re;
  boost::regex empty_re;
  boost::regex table_matches_re;
  boost::regex process_matches_re;
  boost::regex schema_matches_re;

  try
  {
    comment_re= comment_regex;
    empty_re= empty_regex;
    table_matches_re= table_match_regex;
    process_matches_re= process_match_regex;
    schema_matches_re= schema_match_regex;
  }   
  catch (const std::exception &e)
  {
    error << e.what();
    return false;
  }

  if (! file.is_open())
  {
    error << "Unable to open regex policy file: " << policy_file.string();
    return false;
  }

  int lines= 0;
  try
  {
    while (! file.eof())
    {
      ++lines;
      string line;
      getline(file, line);
      if (boost::regex_match(line, comment_re))
      {
        continue;
      }
      if (boost::regex_match(line, empty_re))
      {
        continue;
      }
      boost::smatch matches;
      PolicyItemList *policies;
      if (boost::regex_match(line, matches, table_matches_re, boost::match_extra))
      {
        policies= &table_policies;
      }
      else if (boost::regex_match(line, matches, process_matches_re, boost::match_extra))
      {
        policies= &process_policies;
      }
      else if (boost::regex_match(line, matches, schema_matches_re, boost::match_extra))
      {
        policies= &schema_policies;
      }
      else
      {
        throw std::exception();
      }
      string user_regex;
      string object_regex;
      string action;
      user_regex= matches[MATCH_REGEX_USER_POS];
      object_regex= matches[MATCH_REGEX_OBJECT_POS];
      action= matches[MATCH_REGEX_ACTION_POS];
      PolicyItem *i;
      try
      {
        i= new PolicyItem(user_regex, object_regex, action);
      }
      catch (const std::exception &e)
      {
        error << "Bad policy item: user=" << user_regex << " object=" << object_regex << " action=" << action;
        throw std::exception();
      }
      policies->push_back(i);
    }
    return true;
  }
  catch (const std::exception &e)
  {
    /* On any non-EOF break, unparseable line */
    error << "Unable to parse line " << lines << " of policy file " << policy_file.string() << ":" << e.what();
    return false;
  }
}

void clearPolicyItemList(PolicyItemList policies)
{
  for (PolicyItemList::iterator x= policies.begin() ; x != policies.end() ; ++x)
  {
    delete *x;
    *x= NULL;
  }
} 

Policy::~Policy()
{
  clearPolicyItemList(table_policies);
  clearPolicyItemList(process_policies);
  clearPolicyItemList(schema_policies);
  delete table_check_cache;
  delete process_check_cache;
  delete schema_check_cache;
}

bool Policy::restrictObject(const drizzled::identifier::User &user_ctx,
                                   const string &obj, const PolicyItemList &policies,
                                   CheckMap **check_cache)
{
  CheckItem c(user_ctx.username(), obj, check_cache);
  if (!c.hasCachedResult())
  {
    PolicyItemList::const_iterator m= find_if(policies.begin(), policies.end(), c);
    if (m != policies.end())
    {
      c.setCachedResult((*m)->isRestricted());
    }
    else
    {
      /* TODO: make default action configurable */
      c.setCachedResult(false);
    }
  }
  return c.getCachedResult();
}

bool Policy::restrictSchema(const drizzled::identifier::User &user_ctx,
                                   const drizzled::identifier::Schema& schema)
{
  return restrictObject(user_ctx, schema.getSchemaName(), schema_policies, &schema_check_cache);
}

bool Policy::restrictProcess(const drizzled::identifier::User &user_ctx,
                                    const drizzled::identifier::User &session_ctx)
{
  return restrictObject(user_ctx, session_ctx.username(), process_policies, &process_check_cache);
}

bool Policy::restrictTable(const drizzled::identifier::User& user_ctx,
                             const drizzled::identifier::Table& table)
{
  return restrictObject(user_ctx, table.getTableName(), table_policies, &table_check_cache);
}

bool CheckItem::operator()(PolicyItem *p)
{
  if (p->userMatches(user))
  {
    errmsg_printf(error::INSPECT, _("User %s matches regex\n"), user.c_str());
    if (p->objectMatches(object))
    {
      errmsg_printf(error::INSPECT, _("Object %s matches regex %s (%s)\n"), 
          object.c_str(),
          p->getObject().c_str(),
          p->getAction());
      return true;
    }
    errmsg_printf(error::INSPECT, _("Object %s NOT restricted by regex %s (%s)\n"), 
        object.c_str(),
        p->getObject().c_str(),
        p->getAction());
  }
  return false;
}

CheckItem::CheckItem(const std::string &user_in, const std::string &obj_in, CheckMap **check_cache_in)
  : user(user_in), object(obj_in), has_cached_result(false), check_cache(check_cache_in)
{
  CheckMap::iterator check_val;
  std::stringstream keystream;
  keystream << user << "_" << object;
  key= keystream.str();

  /* using RCU to only need to lock when updating the cache */
  if ((*check_cache) && (check_val= (*check_cache)->find(key)) != (*check_cache)->end())
  {
    setCachedResult(check_val->second);
  }
}

void CheckItem::setCachedResult(bool result)
{
  // TODO: make the mutex per-cache
  boost::mutex::scoped_lock lock(check_cache_mutex, boost::defer_lock);
  lock.lock();

  // Copy the current one
  CheckMap* new_cache= *check_cache ? new CheckMap(**check_cache) : new CheckMap;

  // Update it
  (*new_cache)[key]= result;
  // Replace old
  CheckMap* old_cache= *check_cache;
  *check_cache= new_cache;

  lock.unlock();
  has_cached_result= true;
  cached_result= result;

  delete old_cache;
}

} /* namespace regex_policy */

DRIZZLE_PLUGIN(regex_policy::init, NULL, regex_policy::init_options);
