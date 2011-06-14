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

#include <bitset>

namespace drizzled {

/* Bits from testflag */

namespace debug {
enum flag_t
{
  PRINT_CACHED_TABLES= 1,
  NO_KEY_GROUP,
  MIT_THREAD,
  KEEP_TMP_TABLES,
  READCHECK, /**< Force use of readcheck */
  NO_EXTRA,
  CORE_ON_SIGNAL, /**< Give core if signal */
  NO_STACKTRACE,
  ALLOW_SIGINT, /**< Allow sigint on threads */
  SYNCHRONIZATION /**< get server to do sleep in some places */
};

class Flags
{
public:
  typedef std::bitset<12> Options;

  static inline Flags &singleton()
  {
    static Flags _singleton;

    return _singleton;
  }

  inline Options &options()
  {
    return _options;
  }

private:
  Options _options;
};

} // namespace debug

debug::Flags::Options &getDebug();

} /* namespace drizzled */

