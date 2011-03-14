/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Padraig O'Sullivan
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

#pragma once

#include <drizzled/atomics.h>
#include <drizzled/plugin/plugin.h>

#include <drizzled/visibility.h>

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
class DRIZZLED_API QueryRewriter : public Plugin
{

public:

  explicit QueryRewriter(std::string name_arg)
    : 
      Plugin(name_arg, "QueryRewriter")
  {}

  virtual ~QueryRewriter() {}

  /**
   * Rewrite a query in the form of a std::string
   *
   * @param[in] schema the schema the current session is in
   * @param[out] to_rewrite string representing the query to rewrite
   */
  virtual void rewrite(const std::string &schema, std::string &to_rewrite)= 0; 

  static bool addPlugin(QueryRewriter *in_rewriter);
  static void removePlugin(QueryRewriter *in_rewriter);

  /**
   * Take a query to be rewritten and go through all the rewriters that are registered as plugins
   * and let the enabled ones rewrite they query.
   * TODO: does it make sense to have multiple rewriters?
   *
   * @param[in] schema the schema the current session is
   * @param[out] to_rewrite the query to rewrite
   */
  static void rewriteQuery(const std::string &schema, std::string &to_rewrite);

private:

  QueryRewriter();
  QueryRewriter(const QueryRewriter&);
  QueryRewriter& operator=(const QueryRewriter&);


};

} /* namespace plugin */

} /* namespace drizzled */

