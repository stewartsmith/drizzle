/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Padraig O'Sullivan
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include "drizzled/sql_select.h"
#include "drizzled/join_table.h"
#include "drizzled/optimizer/access_method.h"
#include "drizzled/optimizer/access_method_factory.h"
#include "drizzled/optimizer/access_method/system.h"
#include "drizzled/optimizer/access_method/const.h"
#include "drizzled/optimizer/access_method/unique_index.h"
#include "drizzled/optimizer/access_method/index.h"
#include "drizzled/optimizer/access_method/scan.h"

#include <boost/shared_ptr.hpp>

using namespace drizzled;

boost::shared_ptr<optimizer::AccessMethod>
optimizer::AccessMethodFactory::createAccessMethod(enum access_method type)
{
  boost::shared_ptr<optimizer::AccessMethod> am_ret;

  switch (type)
  {
  case AM_SYSTEM:
    am_ret.reset(new optimizer::System());
    break;
  case AM_CONST:
    am_ret.reset(new optimizer::Const());
    break;
  case AM_EQ_REF:
    am_ret.reset(new optimizer::UniqueIndex());
    break;
  case AM_REF_OR_NULL:
  case AM_REF:
    am_ret.reset(new optimizer::Index());
    break;
  case AM_ALL:
    am_ret.reset(new optimizer::Scan());
    break;
  default:
    break;
  }
  
  return am_ret;
}

