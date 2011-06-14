/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/statement/show_warnings.h>

#include <bitset>

using namespace std;

namespace drizzled
{

bool statement::ShowWarnings::execute()
{
  bitset<DRIZZLE_ERROR::NUM_ERRORS> warning_levels;
  warning_levels.set(DRIZZLE_ERROR::WARN_LEVEL_NOTE);
  warning_levels.set(DRIZZLE_ERROR::WARN_LEVEL_WARN);
  warning_levels.set(DRIZZLE_ERROR::WARN_LEVEL_ERROR);
  bool res= show_warnings(&session(), warning_levels);

  return res;
}

} /* namespace drizzled */

