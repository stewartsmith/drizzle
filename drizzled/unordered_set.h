/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor
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

/**
 * @file
 *
 * Includes either std::unordered_set if we have it, or boost::unordered_set
 * if we don't. Puts them into the drizzled namespace
 */

#ifndef DRIZZLED_UNORDERED_SET_H
#define DRIZZLED_UNORDERED_SET_H

#ifdef HAVE_STD_UNORDERED_SET
# include <unordered_set>
#else
# ifdef HAVE_TR1_UNORDERED_SET
#  include <tr1/unordered_set>
# else
#  ifdef HAVE_BOOST_UNORDERED_SET
#   include <boost/unordered_set.hpp>
#  else
#   include <set>
#   include <functional>
#  endif
# endif
#endif

namespace drizzled {

#ifdef HAVE_STD_UNORDERED_SET
using std::unordered_set;
using std::hash;
#else
# ifdef HAVE_TR1_UNORDERED_SET
using std::tr1::unordered_set;
using std::tr1::hash;
# else
#  ifdef HAVE_BOOST_UNORDERED_SET
using boost::unordered_set;
using boost::hash;
#  else

template <typename Key,
          typename HashFcn = std::less<Key>,
          typename EqualKey = int >
class unordered_set :
  public std::set<Key, HashFcn>
{ 
public:
  void rehash(size_t)
  { }
};

#  endif /* HAVE_BOOST_UNORDERED_SET else */
# endif /* HAVE_TR1_UNORDERED_SET else */
#endif /* HAVE_STD_UNORDERED_SET else */


}  /* namespace drizzled */

#endif /* DRIZZLED_UNORDERED_SET_H */
