/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 ?
 *
 *  Authors:
 *
 *    Padraig O'Sullivan <posullivan@akibainc.com>
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

#ifndef DRIZZLED_PLUGIN_QUERY_REWRITE_H
#define DRIZZLED_PLUGIN_QUERY_REWRITE_H

#include "drizzled/atomics.h"
#include "drizzled/plugin/plugin.h"

/**
 * @file Defines the API for a QueryRewriter.  
 */


namespace drizzled
{

namespace plugin
{

/**
 * Class which rewrites queries
 */
class QueryRewriter : public Plugin
{

  QueryRewriter();
  QueryRewriter(const QueryRewriter&);
  QueryRewriter& operator=(const QueryRewriter&);
  atomic<bool> is_enabled;

public:

  explicit QueryRewriter(std::string name_arg)
    : 
      Plugin(name_arg, "QueryRewriter")
  {
    is_enabled= false;
  }

  virtual ~QueryRewriter() {}

  /**
   * Rewrite a query in the form of a std::string
   *
   * @param[out] to_rewrite string representing the query to rewrite
   */
  virtual void rewrite(std::string &to_rewrite)= 0; 

  static bool addPlugin(QueryRewriter *in_rewriter);
  static void removePlugin(QueryRewriter *in_rewriter);

  virtual bool isEnabled() const
  {
    return is_enabled;
  }

  virtual void enable()
  {
    is_enabled= true;
  }

  virtual void disable()
  {
    is_enabled= false;
  }

  static void rewriteQuery(std::string &to_rewrite);

};

} /* namespace plugin */

} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_QUERY_REWRITE_H */
