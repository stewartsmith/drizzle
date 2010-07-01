/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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
#include "drizzled/plugin/query_cache.h"
#include "drizzled/errmsg_print.h"

#include "drizzled/gettext.h"

#include <algorithm>
#include <vector>

class Session;

using namespace std;

namespace drizzled
{

vector<plugin::QueryCache *> all_query_cache;

/* Namespaces are here to prevent global symbol clashes with these classes */

class TryFetchAndSendIterate
 : public unary_function<plugin::QueryCache *, bool>
{
  Session *session;
public:
  TryFetchAndSendIterate(Session *session_arg) :
    unary_function<plugin::QueryCache *, bool>(),
    session(session_arg) { }

  inline result_type operator()(argument_type handler)
  {
    if (handler->tryFetchAndSend(session))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("qcache plugin '%s' try_fetch_and_send() failed \r"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};

class PrepareResultsetIterate
 : public unary_function<plugin::QueryCache *, bool>
{
  Session *session;
public:
  PrepareResultsetIterate(Session *session_arg) :
    unary_function<plugin::QueryCache *, bool>(), session(session_arg) { }

  inline result_type operator()(argument_type handler)
  {
    if (handler->prepareResultset(session))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("qcache plugin '%s' prepareResultset() failed\r"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};

class SetResultsetIterate
 : public unary_function<plugin::QueryCache *, bool>
{
  Session *session;
public:
  SetResultsetIterate(Session *session_arg) :
    unary_function<plugin::QueryCache *, bool>(),
    session(session_arg) { }

  inline result_type operator()(argument_type handler)
  {

    if (handler->setResultset(session))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("qcache plugin '%s' setResultset() failed\r"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};

class InsertRecordIterate
 : public unary_function<plugin::QueryCache *, bool>
{
  Session *session;
  List<Item> &item;
public:
  InsertRecordIterate(Session *session_arg, List<Item> &item_arg) :
    unary_function<plugin::QueryCache *, bool>(),
    session(session_arg), item(item_arg) { }

  inline result_type operator()(argument_type handler)
  {

    if (handler->insertRecord(session, item))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("qcache plugin '%s'  insertRecord() failed\r"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};



bool plugin::QueryCache::addPlugin(plugin::QueryCache *handler)
{
  all_query_cache.push_back(handler);
  return false;
}

void plugin::QueryCache::removePlugin(plugin::QueryCache *handler)
{
  all_query_cache.erase(find(all_query_cache.begin(), all_query_cache.end(),
                        handler));
}


bool plugin::QueryCache::tryFetchAndSendDo(Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<plugin::QueryCache *>::iterator iter=
    find_if(all_query_cache.begin(), all_query_cache.end(),
            TryFetchAndSendIterate(session));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}

bool plugin::QueryCache::prepareResultsetDo(Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<plugin::QueryCache *>::iterator iter=
    find_if(all_query_cache.begin(), all_query_cache.end(),
            PrepareResultsetIterate(session));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}

bool plugin::QueryCache::setResultsetDo(Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<plugin::QueryCache *>::iterator iter=
    find_if(all_query_cache.begin(), all_query_cache.end(),
            SetResultsetIterate(session));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}

bool plugin::QueryCache::insertRecordDo(Session *session, List<Item> &items)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<plugin::QueryCache *>::iterator iter=
    find_if(all_query_cache.begin(), all_query_cache.end(),
            InsertRecordIterate(session, items));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}


} /* namespace drizzled */
