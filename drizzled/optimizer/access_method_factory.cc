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

#include <config.h>
#include <drizzled/optimizer/access_method.h>
#include <drizzled/optimizer/access_method_factory.h>
#include <drizzled/optimizer/access_method/system.h>
#include <drizzled/optimizer/access_method/const.h>
#include <drizzled/optimizer/access_method/unique_index.h>
#include <drizzled/optimizer/access_method/index.h>
#include <drizzled/optimizer/access_method/scan.h>

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

using namespace drizzled;

optimizer::AccessMethod::ptr optimizer::AccessMethodFactory::create(access_method type)
{
  switch (type)
  {
  case AM_SYSTEM:
    return boost::make_shared<optimizer::System>();
  case AM_CONST:
    return boost::make_shared<optimizer::Const>();
  case AM_EQ_REF:
    return boost::make_shared<optimizer::UniqueIndex>();
  case AM_REF_OR_NULL:
  case AM_REF:
    return boost::make_shared<optimizer::Index>();
  case AM_ALL:
    return boost::make_shared<optimizer::Scan>();
  default:
    assert(false);
  }
  return optimizer::AccessMethod::ptr();
}
