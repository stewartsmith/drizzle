/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (c) 2010, Brian Aker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of Patrick Galbraith nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
  Some sections of this code came from the Boost examples.
*/

#ifndef DRIZZLED_UTIL_STRING_H
#define DRIZZLED_UTIL_STRING_H

#include <string>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/functional/hash.hpp>

namespace drizzled
{

namespace util
{


namespace string {
typedef boost::shared_ptr<std::string> shared_ptr;
typedef boost::shared_ptr<const std::string> const_shared_ptr;
}

struct insensitive_equal_to : std::binary_function<std::string, std::string, bool>
{
  bool operator()(std::string const& x, std::string const& y) const
  {
    return boost::algorithm::iequals(x, y, std::locale());
  }
};

struct insensitive_hash : std::unary_function<std::string, std::size_t>
{
  std::size_t operator()(std::string const& x) const
  {
    std::size_t seed = 0;
    std::locale locale;

    for(std::string::const_iterator it = x.begin(); it != x.end(); ++it)
    {
      boost::hash_combine(seed, std::toupper(*it, locale));
    }

    return seed;
  }
};

struct sensitive_hash : std::unary_function< std::vector<char>, std::size_t>
{
  std::size_t operator()(std::vector<char> const& x) const
  {
    std::size_t seed = 0;

    for(std::vector<char>::const_iterator it = x.begin(); it != x.end(); ++it)
    {
      boost::hash_combine(seed, *it);
    }

    return seed;
  }
};

} /* namespace util */
} /* namespace drizzled */

#endif /* DRIZZLED_UTIL_STRING_H */
