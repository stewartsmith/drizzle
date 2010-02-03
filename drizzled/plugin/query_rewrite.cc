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

#include "config.h"
#include "drizzled/plugin/query_rewrite.h"
#include "drizzled/gettext.h"
#include "drizzled/plugin/registry.h"

#include <vector>

using namespace std;

namespace drizzled
{

static plugin::QueryRewriter *handler= NULL;


bool plugin::QueryRewriter::addPlugin(plugin::QueryRewriter *in_rewriter)
{
  if (in_rewriter != NULL)
  {
    handler= in_rewriter;
  }
  return false;
}


void plugin::QueryRewriter::removePlugin(plugin::QueryRewriter *in_rewriter)
{
  if (in_rewriter != NULL)
  {
    handler= NULL;
  }
}


/*
 * This is the QueryRewriter::rewrite entry point.
 * This gets called from with the Drizzle kernel.
 */
void plugin::QueryRewriter::rewriteQuery(std::string &to_rewrite)
{
  if (handler != NULL)
  {
    handler->rewrite(to_rewrite);
  }
}

} /* namespace drizzled */
