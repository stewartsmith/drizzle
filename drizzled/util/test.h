/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_UTIL_TEST_H
#define DRIZZLED_UTIL_TEST_H

#if defined(__cplusplus)

namespace drizzled
{

template <class T>
inline bool test(const T a)
{
  return a ? true : false;
}

template <class T, class U>
inline bool test_all_bits(const T a, const U b)
{
  return ((a & b) == b);
}

} /* namespace drizzled */

#else
# define test(a)    ((a) ? 1 : 0)
# define test_all_bits(a,b) (((a) & (b)) == (b))
#endif

#endif /* DRIZZLED_UTIL_TEST_H */
