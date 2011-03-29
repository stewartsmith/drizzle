/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Djellel Eddine Difallah
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
#include <drizzled/plugin/query_cache.h>
#include <drizzled/errmsg_print.h>

#include <drizzled/gettext.h>

#include <algorithm>
#include <vector>

namespace drizzled {

typedef std::vector<plugin::QueryCache *> QueryCaches;
QueryCaches all_query_cache;

/* Namespaces are here to prevent global symbol clashes with these classes */

class IsCachedIterate
 : public std::unary_function<plugin::QueryCache *, bool>
{
  Session *session;
public:
  IsCachedIterate(Session* session_arg) :
    std::unary_function<plugin::QueryCache *, bool>(),
    session(session_arg) { }

  inline result_type operator()(argument_type handler)
  {
    return handler->doIsCached(session);
  }
};

bool plugin::QueryCache::isCached(Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  QueryCaches::iterator iter=
    std::find_if(all_query_cache.begin(), all_query_cache.end(),
            IsCachedIterate(session));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}


class SendCachedResultsetIterate
 : public std::unary_function<plugin::QueryCache *, bool>
{
  Session *session;
public:
  SendCachedResultsetIterate(Session *session_arg) :
    std::unary_function<plugin::QueryCache *, bool>(),
    session(session_arg) { }

  inline result_type operator()(argument_type handler)
  {
    return handler->doSendCachedResultset(session);
  }
};
bool plugin::QueryCache::sendCachedResultset(Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  QueryCaches::iterator iter=
    std::find_if(all_query_cache.begin(), all_query_cache.end(),
                 SendCachedResultsetIterate(session));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}

class PrepareResultsetIterate : public std::unary_function<plugin::QueryCache *, bool>
{
  Session *session;
public:
  PrepareResultsetIterate(Session *session_arg) :
    std::unary_function<plugin::QueryCache *, bool>(),
    session(session_arg) { }

  inline result_type operator()(argument_type handler)
  {
    return handler->doPrepareResultset(session);
  }
};
bool plugin::QueryCache::prepareResultset(Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  QueryCaches::iterator iter=
    std::find_if(all_query_cache.begin(), all_query_cache.end(),
                 PrepareResultsetIterate(session));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}

class SetResultsetIterate : public std::unary_function<plugin::QueryCache *, bool>
{
  Session *session;
public:
  SetResultsetIterate(Session *session_arg) :
    std::unary_function<plugin::QueryCache *, bool>(),
    session(session_arg) { }

  inline result_type operator()(argument_type handler)
  {
    return handler->doSetResultset(session);
  }
};

bool plugin::QueryCache::setResultset(Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  QueryCaches::iterator iter=
    std::find_if(all_query_cache.begin(), all_query_cache.end(),
                 SetResultsetIterate(session));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}

class InsertRecordIterate
 : public std::unary_function<plugin::QueryCache *, bool>
{
  Session *session;
  List<Item> &item;
public:
  InsertRecordIterate(Session *session_arg, List<Item> &item_arg) :
    std::unary_function<plugin::QueryCache *, bool>(),
    session(session_arg), item(item_arg) { }

  inline result_type operator()(argument_type handler)
  {
    return handler->doInsertRecord(session, item);
  }
};
bool plugin::QueryCache::insertRecord(Session *session, List<Item> &items)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  QueryCaches::iterator iter=
    std::find_if(all_query_cache.begin(), all_query_cache.end(),
                 InsertRecordIterate(session, items));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}



bool plugin::QueryCache::addPlugin(plugin::QueryCache *handler)
{
  all_query_cache.push_back(handler);
  return false;
}

void plugin::QueryCache::removePlugin(plugin::QueryCache *handler)
{
  all_query_cache.erase(std::find(all_query_cache.begin(), all_query_cache.end(),
                                  handler));
}

} /* namespace drizzled */
