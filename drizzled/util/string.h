/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2010 Brian Aker
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

#pragma once

#include <utility>
#include <string>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>

#define drizzle_literal_parameter(X) (X), size_t((sizeof(X) - 1))

namespace drizzled {
namespace util {

struct insensitive_equal_to : std::binary_function<std::string, std::string, bool>
{
  bool operator()(std::string const& x, std::string const& y) const
  {
    return boost::algorithm::iequals(x, y);
  }
};

struct insensitive_hash : std::unary_function<std::string, std::size_t>
{
  std::size_t operator()(std::string const& x) const
  {
    std::size_t seed = 0;
    BOOST_FOREACH(std::string::const_reference it, x)
      boost::hash_combine(seed, std::toupper(it));
    return seed;
  }
};

struct sensitive_hash : std::unary_function< std::vector<char>, std::size_t>
{
  std::size_t operator()(std::vector<char> const& x) const
  {
    std::size_t seed = 0;
    BOOST_FOREACH(std::vector<char>::const_reference it, x)
      boost::hash_combine(seed, it);
    return seed;
  }
};

class String
{
public:

  String()
  {
    _bytes.resize(1);
  }

  const char* data() const
  {
    return &_bytes[0];
  }

  char* data()
  {
    return &_bytes[0];
  }

  void assign(const size_t repeat, const char arg)
  {
    _bytes.resize(repeat + 1); // inserts \0 at end
    memset(&_bytes[0], arg, repeat);
    _bytes[repeat]= 0;
  }

  void assign(const char *arg, const size_t arg_size)
  {
    _bytes.resize(arg_size + 1); // +1 for \0
    memcpy(&_bytes[0], arg, arg_size);
    _bytes[arg_size]= 0;
  }

  void append(const char *arg, const size_t arg_size)
  {
    if (not arg or not arg_size)
      return;

    size_t original_size= size();
    if (original_size)
    {
      _bytes.resize(original_size + arg_size + 1); // inserts NULL since string will already have a NULL
      memcpy(&_bytes[original_size], arg, arg_size);
      _bytes[original_size + arg_size]= 0;
    }
    else
    {
      assign(arg, arg_size);
    }
  }

  const char& operator[] (size_t arg) const
  {
    return _bytes[arg];
  }

  char& operator[] (size_t arg)
  {
    return _bytes[arg];
  }

  void clear()
  {
    _bytes.resize(1);
    _bytes[0]= 0;
  }

  size_t size() const
  {
    return _bytes.size() -1;
  }

private:
  std::vector<char> _bytes;
};

} /* namespace util */
} /* namespace drizzled */

