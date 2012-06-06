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


#pragma once

#include <iosfwd>

#include <boost/regex.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>

#include <drizzled/configmake.h>
#include <drizzled/plugin/authorization.h>

namespace fs= boost::filesystem;

namespace regex_policy {

static const fs::path DEFAULT_POLICY_FILE= SYSCONFDIR "/drizzle.policy";

static const uint64_t DEFAULT_MAX_LRU_LENGTH= 16384;
static const uint64_t DEFAULT_MAX_CACHE_BUCKETS= 4096;

static const char *comment_regex = "^[[:space:]]*#.*$";
static const char *empty_regex = "^[[:space:]]*$";
static const char *table_match_regex = "^([^ ]+) table\\=([^ ]+) (ACCEPT|REJECT|ALLOW|DENY)$";
static const char *process_match_regex = "^([^ ]+) process\\=([^ ]+) (ACCEPT|REJECT|ALLOW|DENY)$";
static const char *schema_match_regex = "^([^ ]+) schema\\=([^ ]+) (ACCEPT|REJECT|ALLOW|DENY)$";
/* These correspond to the parenthesis above and must stay in sync */
static const int MATCH_REGEX_USER_POS= 1;
static const int MATCH_REGEX_OBJECT_POS= 2;
static const int MATCH_REGEX_ACTION_POS= 3;


typedef enum 
{
  POLICY_ACCEPT,
  POLICY_DENY
} PolicyAction;

class PolicyItem
{
  const std::string user;
  const std::string object;
  const boost::regex user_re;
  const boost::regex object_re;
  PolicyAction action;
public:
  PolicyItem(const std::string &u, const std::string &obj, const std::string &act) :
    user(u),
    object(obj),
    user_re(u),
    object_re(obj)
  { 
    if ((act == "ACCEPT")||(act == "ALLOW"))
    {
      action = POLICY_ACCEPT;
    }
    else if ((act == "REJECT")||(act == "DENY"))
    {
      action = POLICY_DENY;
    }
    else
    {
      throw std::exception();
    }
  }
  bool userMatches(std::string &str);
  bool objectMatches(std::string &object_id);
  bool isRestricted() const;
  const std::string&getUser() const
  {
    return user;
  }
  const std::string&getObject() const
  {
    return object;
  }
  const char *getAction() const
  {
    return action == POLICY_ACCEPT ? "ALLOW" : "DENY";
  }
};

typedef std::list<PolicyItem *> PolicyItemList;
typedef std::vector<std::string> LruList;
typedef boost::unordered_map<std::string, bool> UnorderedCheckMap;

class CheckMap
{
  LruList lru;
  boost::mutex lru_mutex;
  boost::shared_mutex map_mutex;
  UnorderedCheckMap map;
public:
  bool* find(std::string const&k);
  void insert(std::string const &k, bool v);
  void clear()
  {
        map.clear();
  }
};

class CheckItem
{
  std::string user;
  std::string object;
  std::string key;
  bool has_cached_result;
  bool cached_result;
  CheckMap &check_cache;
public:
  CheckItem(const std::string &u, const std::string &obj, CheckMap &check_cache);
  bool operator()(PolicyItem *p);
  bool hasCachedResult() const
  {
    return has_cached_result;
  }
  bool getCachedResult() const
  {
    return cached_result;
  }
  void setCachedResult(bool result);
};

inline bool PolicyItem::userMatches(std::string &str)
{
  return boost::regex_match(str, user_re);
}

inline bool PolicyItem::objectMatches(std::string &object_id)
{
  return boost::regex_match(object_id, object_re);
}

inline bool PolicyItem::isRestricted() const
{
  return action == POLICY_DENY;
}

class Policy :
  public drizzled::plugin::Authorization
{
public:
  Policy(const std::string &f_path) :
    drizzled::plugin::Authorization("regex_policy"), sysvar_policy_file(f_path), policy_file(f_path), error(),
    table_check_cache(), schema_check_cache(), process_check_cache()
  { }

  virtual bool restrictSchema(const drizzled::identifier::User &user_ctx,
                              const drizzled::identifier::Schema& schema);

  virtual bool restrictProcess(const drizzled::identifier::User &user_ctx,
                               const drizzled::identifier::User &session_ctx);

  virtual bool restrictTable(const drizzled::identifier::User& user_ctx,
                             const drizzled::identifier::Table& table);

  void setPolicies(PolicyItemList new_table_policies, PolicyItemList new_schema_policies, PolicyItemList new_process_policies);
  void clearPolicies();
  std::string& getPolicyFile();
  bool setPolicyFile(std::string& new_policy_file);
  std::stringstream &getError() { return error; }
  ~Policy();
private:
  bool restrictObject(const drizzled::identifier::User &user_ctx,
                                   const std::string &obj, const PolicyItemList &policies,
                                   CheckMap &check_cache);
  std::string sysvar_policy_file;
  fs::path policy_file;
  std::stringstream error;
  PolicyItemList table_policies;
  PolicyItemList schema_policies;
  PolicyItemList process_policies;
  CheckMap table_check_cache;
  CheckMap schema_check_cache;
  CheckMap process_check_cache;
};

} /* namespace regex_policy */

