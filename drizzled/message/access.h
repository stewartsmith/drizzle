/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
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

#pragma once

#include <drizzled/identifier.h>
#include <drizzled/message/access.pb.h>

namespace drizzled {
namespace message {

template<class T> void set_definer(T& reference, const identifier::User &arg)
{
  message::Access *access= reference.mutable_access();
  access->set_definer(arg.username());
}

template<class T> bool has_definer(const T& reference)
{
  if (reference.has_access() and reference.access().has_definer() and (not reference.access().definer().empty()))
  {
    return true;
  }

  return false;
}

template<class T> const char *definer(const T& reference)
{
  if (reference.has_access() and reference.access().has_definer())
  {
    return reference.access().definer().c_str();
  }

  return ""; // Hardcoded because of dependency issue with message library on identifier
}

} /* namespace message */
} /* namespace drizzled */
