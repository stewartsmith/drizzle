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

#include <drizzled/server_includes.h>
#include <drizzled/qcache.h>
#include <drizzled/gettext.h>
#include "drizzled/plugin_registry.h"
#include <vector>

using namespace std;

static vector<QueryCache *> all_query_cache;

void add_query_cache(QueryCache *handler)
{
  all_query_cache.push_back(handler);
}

void remove_query_cache(QueryCache *handler)
{
  all_query_cache.erase(find(all_query_cache.begin(), all_query_cache.end(),
                        handler));
}



/* Namespaces are here to prevent global symbol clashes with these classes */

namespace drizzled {
namespace query_cache {

class TryFetchAndSendIterate
 : public unary_function<QueryCache *, bool>
{
  Session *session;
  bool is_transactional;
public:
  TryFetchAndSendIterate(Session *session_arg, bool is_transactional_arg) :
    unary_function<QueryCache *, bool>(),
    session(session_arg), is_transactional(is_transactional_arg) { }

  inline result_type operator()(argument_type handler)
  {
    if (handler->try_fetch_and_send(session, is_transactional))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("qcache plugin '%s' try_fetch_and_send() failed"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};

class SetIterate
 : public unary_function<QueryCache *, bool>
{
  Session *session;
  bool is_transactional;
public:
  SetIterate(Session *session_arg, bool is_transactional_arg) :
    unary_function<QueryCache *, bool>(),
    session(session_arg), is_transactional(is_transactional_arg) { }

  inline result_type operator()(argument_type handler)
  {

    if (handler->set(session, is_transactional))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("qcache plugin '%s' set() failed"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};

class InvalidateTableIterate
 : public unary_function<QueryCache *, bool>
{
  Session *session;
  bool is_transactional;
public:
  InvalidateTableIterate(Session *session_arg, bool is_transactional_arg) :
    unary_function<QueryCache *, bool>(),
    session(session_arg), is_transactional(is_transactional_arg) { }

  inline result_type operator()(argument_type handler)
  {

    if (handler->invalidate_table(session, is_transactional))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("qcache plugin '%s' invalidate_table() failed"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};


class InvalidateDbIterate
 : public unary_function<QueryCache *, bool>
{
  Session *session;
  const char *dbname;
  bool is_transactional;
public:
  InvalidateDbIterate(Session *session_arg, const char *dbname_arg,
                      bool is_transactional_arg) :
    unary_function<QueryCache *, bool>(),
    session(session_arg), dbname(dbname_arg),
    is_transactional(is_transactional_arg) { }

  inline result_type operator()(argument_type handler)
  {
    if (handler->invalidate_db(session, dbname, is_transactional))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("qcache plugin '%s' invalidate_db() failed"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};

class FlushIterate
 : public unary_function<QueryCache *, bool>
{
  Session *session;
public:
  FlushIterate(Session *session_arg) :
    unary_function<QueryCache *, bool>(), session(session_arg) { }

  inline result_type operator()(argument_type handler)
  {
    if (handler->flush(session))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("qcache plugin '%s' flush() failed"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};

} /* namespace query_cache */
} /* namespace drizzled */

using namespace drizzled::query_cache;

/*
  Following functions:

    drizzle_qcache_try_fetch_and_send();
    drizzle_qcache_set();
    drizzle_qcache_invalidate_table();
    drizzle_qcache_invalidate_db();
    drizzle_qcache_flush();

  are the entry points to the query cache plugin that is called by the
  rest of the Drizzle server code.
*/

bool drizzle_qcache_try_fetch_and_send(Session *session, bool transactional)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<QueryCache *>::iterator iter=
    find_if(all_query_cache.begin(), all_query_cache.end(),
            TryFetchAndSendIterate(session, transactional));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}

bool drizzle_qcache_set(Session *session, bool transactional)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<QueryCache *>::iterator iter=
    find_if(all_query_cache.begin(), all_query_cache.end(),
            SetIterate(session, transactional));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}

bool drizzle_qcache_invalidate_table(Session *session, bool transactional)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<QueryCache *>::iterator iter=
    find_if(all_query_cache.begin(), all_query_cache.end(),
            InvalidateTableIterate(session, transactional));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}

bool drizzle_qcache_invalidate_db(Session *session, const char *dbname,
                                  bool transactional)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<QueryCache *>::iterator iter=
    find_if(all_query_cache.begin(), all_query_cache.end(),
            InvalidateDbIterate(session, dbname, transactional));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}

bool drizzle_qcache_flush(Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<QueryCache *>::iterator iter=
    find_if(all_query_cache.begin(), all_query_cache.end(),
            FlushIterate(session));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_query_cache.end();
}
