/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Jay Pipes <jaypipes@gmail.com>
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

#include <drizzled/cached_directory.h>

#include <drizzled/definitions.h>
#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin/xa_storage_engine.h>
#include <drizzled/xid.h>

#include <string>
#include <vector>
#include <algorithm>
#include <functional>

namespace drizzled
{

namespace plugin
{

static std::vector<XaStorageEngine *> vector_of_xa_engines;

XaStorageEngine::XaStorageEngine(const std::string &name_arg,
                                 const std::bitset<HTON_BIT_SIZE> &flags_arg) :
  TransactionalStorageEngine(name_arg, flags_arg)
{}

XaStorageEngine::~XaStorageEngine()
{}

bool XaStorageEngine::addPlugin(XaStorageEngine *engine)
{
  vector_of_xa_engines.push_back(engine);

  return TransactionalStorageEngine::addPlugin(engine) &&
         XaResourceManager::addPlugin(engine);
}

void XaStorageEngine::removePlugin(XaStorageEngine *engine)
{
  vector_of_xa_engines.clear();
  TransactionalStorageEngine::removePlugin(engine);
  XaResourceManager::removePlugin(engine);
}

} /* namespace plugin */
} /* namespace drizzled */
