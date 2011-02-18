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

#include <config.h>
#include <drizzled/plugin/query_rewrite.h>
#include <drizzled/gettext.h>

#include <algorithm>
#include <vector>

namespace drizzled
{

namespace plugin
{

std::vector<plugin::QueryRewriter *> all_rewriters;


bool QueryRewriter::addPlugin(QueryRewriter *in_rewriter)
{
  if (in_rewriter != NULL)
  {
    all_rewriters.push_back(in_rewriter);
  }
  return false;
}


void QueryRewriter::removePlugin(QueryRewriter *in_rewriter)
{
  if (in_rewriter != NULL)
  {
    all_rewriters.erase(std::find(all_rewriters.begin(),
                                  all_rewriters.end(),
                                  in_rewriter));
  }
}


/*
 * This is the QueryRewriter::rewrite entry point.
 * This gets called from within the Drizzle kernel.
 */
void QueryRewriter::rewriteQuery(const std::string &schema, std::string &to_rewrite)
{
  for (std::vector<plugin::QueryRewriter *>::iterator iter= all_rewriters.begin();
       iter != all_rewriters.end();
       ++iter)
  {
    (*iter)->rewrite(schema, to_rewrite);
  }
}

} /* namespace plugin */

} /* namespace drizzled */
