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

#include <boost/foreach.hpp>
#include <boost/unordered_set.hpp>
#include <boost/thread/locks.hpp>

#include <drizzled/item.h>
#include <drizzled/plugin/authorization.h>
#include <drizzled/module/option_map.h>

#include "policy.h"

#include <fstream>

namespace po= boost::program_options;

using namespace std;
using namespace drizzled;

namespace regex_policy
{

uint64_t max_cache_buckets= DEFAULT_MAX_CACHE_BUCKETS;
uint64_t max_lru_length= DEFAULT_MAX_LRU_LENGTH;
bool updatePolicyFile(Session *, set_var *);
bool parsePolicyFile(std::string, PolicyItemList&, PolicyItemList&, PolicyItemList&);
Policy *policy= NULL;

bool updatePolicyFile(Session *, set_var* var)
{
  if (not var->value->str_value.empty())
  {
    std::string newPolicyFile(var->value->str_value.data());
    if (policy->setPolicyFile(newPolicyFile))
      return false; //success
    else
      return true; // error
  }
  errmsg_printf(error::ERROR, _("regex_policy file cannot be NULL"));
  return true; // error
}

bool parsePolicyFile(std::string new_policy_file, PolicyItemList& table_policies_dummy, PolicyItemList& schema_policies_dummy, PolicyItemList& process_policies_dummy)
{
  ifstream file(new_policy_file.c_str());
  boost::regex comment_re;
  boost::regex empty_re;
  boost::regex table_matches_re;
  boost::regex process_matches_re;
  boost::regex schema_matches_re;
  table_policies_dummy.clear();
  schema_policies_dummy.clear();
  process_policies_dummy.clear();

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
    errmsg_printf(error::ERROR, _(e.what()));
    return false;
  }

  if (! file.is_open())
  {
    string error_msg= "Unable to open regex policy file: " + new_policy_file;
    errmsg_printf(error::ERROR, _(error_msg.c_str()));
    return false;
  }

  int lines= 0;
  try
  {
    for (string line; getline(file, line); )
    {
      ++lines;
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
        policies= &table_policies_dummy;
      }
      else if (boost::regex_match(line, matches, process_matches_re, boost::match_extra))
      {
        policies= &process_policies_dummy;
      }
      else if (boost::regex_match(line, matches, schema_matches_re, boost::match_extra))
      {
        policies= &schema_policies_dummy;
      }
      else
      {
        throw std::exception();
      }
      string user_regex= matches[MATCH_REGEX_USER_POS];
      string object_regex= matches[MATCH_REGEX_OBJECT_POS];
      string action= matches[MATCH_REGEX_ACTION_POS];
      try
      {
        policies->push_back(new PolicyItem(user_regex, object_regex, action));
      }
      catch (const std::exception &e)
      {
	string error_msg= "Bad policy item: user=" + user_regex + " object=" + object_regex + " action=" + action;
        errmsg_printf(error::ERROR, _(error_msg.c_str()));
        throw std::exception();
      }
    }
    return true;
  }
  catch (const std::exception &e)
  {
    /* On any non-EOF break, unparseable line */
    string error_msg= "Unable to parse policy file " + new_policy_file + ":" + e.what();
    errmsg_printf(error::ERROR, _(error_msg.c_str()));
    return false;
  }

}

static int init(module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  max_cache_buckets= vm["max-cache-buckets"].as<uint64_t>();
  if (max_cache_buckets < 1)
  {
    errmsg_printf(error::ERROR, _("max-cache-buckets is too low, must be greater than 0"));
    return 1;
  }
  max_lru_length= vm["max-lru-length"].as<uint64_t>();
  if (max_lru_length < 1)
  {
    errmsg_printf(error::ERROR, _("max-lru-length is too low, must be greater than 0"));
    return 1;
  }
  policy= new Policy(vm["policy"].as<string>());
  if (!policy->setPolicyFile(policy->getPolicyFile()))
  {
    errmsg_printf(error::ERROR, _("Could not load regex policy file: %s\n"),
                  (policy ? policy->getError().str().c_str() : _("Unknown")));
    delete policy;
    return 1;
  }
  context.add(policy);
  context.registerVariable(new sys_var_std_string("policy", policy->getPolicyFile(), NULL, &updatePolicyFile));

  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("policy",
      po::value<string>()->default_value(DEFAULT_POLICY_FILE.string()),
      N_("File to load for regex authorization policies"));
  context("max-cache-buckets",
      po::value<uint64_t>()->default_value(DEFAULT_MAX_CACHE_BUCKETS),
      N_("Maximum buckets for authorization cache"));
  context("max-lru-length",
      po::value<uint64_t>()->default_value(DEFAULT_MAX_LRU_LENGTH),
      N_("Maximum number of LRU entries to track at once"));
}

void Policy::setPolicies(PolicyItemList new_table_policies, PolicyItemList new_schema_policies, PolicyItemList new_process_policies)
{
  policy->clearPolicies();

  for (PolicyItemList::iterator it= new_table_policies.begin(); it!= new_table_policies.end(); it++)
    table_policies.push_back(*it);

  for (PolicyItemList::iterator it= new_schema_policies.begin(); it!= new_schema_policies.end(); it++)
    schema_policies.push_back(*it);

  for (PolicyItemList::iterator it= new_process_policies.begin(); it!= new_process_policies.end(); it++)
    process_policies.push_back(*it);
}

std::string& Policy::getPolicyFile()
{
  return sysvar_policy_file;
}

bool Policy::setPolicyFile(std::string &new_policy_file)
{
  if (new_policy_file.empty())
  {
    errmsg_printf(error::ERROR, _("regex_policy file cannot be an empty string"));
    return false;  // error
  }

  PolicyItemList new_table_policies;
  PolicyItemList new_schema_policies;
  PolicyItemList new_process_policies;
  if(parsePolicyFile(new_policy_file, new_table_policies, new_schema_policies, new_process_policies))
  {
    policy->setPolicies(new_table_policies, new_schema_policies, new_process_policies);
    sysvar_policy_file= new_policy_file;
    fs::path newPolicyFile(getPolicyFile());
    policy_file= newPolicyFile;
    return true;  // success
  }
  return false;  // error
}

static void clearPolicyItemList(PolicyItemList& policies)
{
  BOOST_FOREACH(PolicyItem* x, policies)
  {
    delete x;
  }
}

Policy::~Policy()
{
  clearPolicyItemList(table_policies);
  clearPolicyItemList(process_policies);
  clearPolicyItemList(schema_policies);
}

/*
This function will be called when the policy file needs to be reloaded.
This deletes all the policies stored and cached.
*/
void Policy::clearPolicies()
{
  table_policies.clear();
  process_policies.clear();
  schema_policies.clear();
  table_check_cache.clear();
  process_check_cache.clear();
  schema_check_cache.clear();
}


bool Policy::restrictObject(const drizzled::identifier::User &user_ctx,
                                   const string &obj, const PolicyItemList &policies,
                                   CheckMap &check_cache)
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
  return restrictObject(user_ctx, schema.getSchemaName(), schema_policies, schema_check_cache);
}

bool Policy::restrictProcess(const drizzled::identifier::User &user_ctx,
                                    const drizzled::identifier::User &session_ctx)
{
  return restrictObject(user_ctx, session_ctx.username(), process_policies, process_check_cache);
}

bool Policy::restrictTable(const drizzled::identifier::User& user_ctx,
                             const drizzled::identifier::Table& table)
{
  return restrictObject(user_ctx, table.getTableName(), table_policies, table_check_cache);
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

CheckItem::CheckItem(const std::string &user_in, const std::string &obj_in, CheckMap &check_cache_in)
  : user(user_in), object(obj_in), has_cached_result(false), check_cache(check_cache_in)
{
  key= user + "_" + object;

  if (bool* check_val= check_cache.find(key))
  {
    /* It was in the cache, no need to do any more lookups */
    cached_result= *check_val;
    has_cached_result= true;
  }
}

bool* CheckMap::find(std::string const &k)
{
  /* tack on to LRU list */
  boost::mutex::scoped_lock lock(lru_mutex);
  lru.push_back(k);
  if (lru.size() > max_lru_length)
  {
    /* Fold all of the oldest entries into a single list at the front */
    uint64_t size_halfway= lru.size() / 2;
    LruList::iterator halfway= lru.begin();
    halfway += size_halfway;
    boost::unordered_set<std::string> uniqs;
    uniqs.insert(lru.begin(), halfway);

    /* If we can save space, drop the oldest half */
    if (size_halfway < uniqs.size())
    {
      lru.erase(lru.begin(), halfway);

      /* Re-add set elements to front */
      lru.insert(lru.begin(), uniqs.begin(), uniqs.end());
    }
  }
  lock.unlock();
  boost::shared_lock<boost::shared_mutex> map_lock(map_mutex);
  return find_ptr(map, k);
}

void CheckMap::insert(std::string const &k, bool v)
{
  boost::unique_lock<boost::shared_mutex> map_lock(map_mutex);
  /* add our new hotness to the map */
  map[k]=v;
  /* Now prune if necessary */
  if (map.bucket_count() > max_cache_buckets)
  {
    /* Determine LRU key by running through the LRU list */
    boost::unordered_set<std::string> found;

    /* Must unfortunately lock the LRU list while we traverse it */
    boost::mutex::scoped_lock lock(lru_mutex);
    for (LruList::reverse_iterator x= lru.rbegin(); x < lru.rend(); ++x)
    {
      if (found.find(*x) == found.end())
      {
        /* Newly found key */
        if (found.size() >= max_cache_buckets)
        {
          /* Since found is already as big as the cache can be, anything else
             is LRU */
          map.erase(*x);
        }
        else
        {
          found.insert(*x);
        }
      }
    }
    if (map.bucket_count() > max_cache_buckets)
    {
      /* Still too big. */
      if (lru.size())
      {
        /* Just delete the oldest item */
        map.erase(*(lru.begin()));
      }
      else
      {
        /* Nothing to delete, warn */
        errmsg_printf(error::WARN,
            _("Unable to reduce size of cache below max buckets (current buckets=%" PRIu64 ")"),
            static_cast<uint64_t>(map.bucket_count()));
      }
    }
    lru.clear();
    lock.unlock();
  }
}

void CheckItem::setCachedResult(bool result)
{
  check_cache.insert(key, result);
  has_cached_result= true;
  cached_result= result;
}

} /* namespace regex_policy */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "regex_policy",
  "2.1",
  "Clint Byrum",
  N_("Authorization using a regex-matched policy file"),
  PLUGIN_LICENSE_GPL,
  regex_policy::init,
  NULL,
  regex_policy::init_options
}
DRIZZLE_DECLARE_PLUGIN_END;
