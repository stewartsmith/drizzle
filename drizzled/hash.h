/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//
// Deals with the fact that hash_map is not defined everywhere.

#ifndef DRIZZLED_HASH_H
#define DRIZZLED_HASH_H

#include <cstring>
#include <string>

#if defined(HAVE_HASH_MAP) && defined(HAVE_HASH_SET)
#include HASH_MAP_H
#include HASH_SET_H
#else
#define MISSING_HASH
#include <map>
#include <set>
#endif

namespace drizzled {

#ifdef MISSING_HASH

// This system doesn't have hash_map or hash_set.  Emulate them using map and
// set.

// Make hash<T> be the same as less<T>.  Note that everywhere where custom
// hash functions are defined in the protobuf code, they are also defined such
// that they can be used as "less" functions, which is required by MSVC anyway.
template <typename Key>
struct hash {
  // Dummy, just to make derivative hash functions compile.
  int operator()(const Key& key) {
    assert(0);
    return 0;
  }

  inline bool operator()(const Key& a, const Key& b) const {
    return a < b;
  }
};

// Make sure char* is compared by value.
template <>
struct hash<const char*> {
  // Dummy, just to make derivative hash functions compile.
  int operator()(const char*) {
    assert(0);
    return 0;
  }

  inline bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) < 0;
  }
};

template <typename Key, typename Data,
          typename HashFcn = hash<Key>,
          typename EqualKey = int >
class hash_map : public std::map<Key, Data, HashFcn> {
};

template <typename Key,
          typename HashFcn = hash<Key>,
          typename EqualKey = int >
class hash_set : public std::set<Key, HashFcn> {
};

#elif defined(_MSC_VER) && !defined(_STLPORT_VERSION)

template <typename Key>
struct hash : public HASH_NAMESPACE::hash_compare<Key> {
};

// MSVC's hash_compare<const char*> hashes based on the string contents but
// compares based on the string pointer.  WTF?
class CstringLess {
 public:
  inline bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) < 0;
  }
};

template <>
struct hash<const char*>
  : public HASH_NAMESPACE::hash_compare<const char*, CstringLess> {
};

template <typename Key, typename Data,
          typename HashFcn = hash<Key>,
          typename EqualKey = int >
class hash_map : public HASH_NAMESPACE::HASH_MAP_CLASS<
    Key, Data, HashFcn> {
};

template <typename Key,
          typename HashFcn = hash<Key>,
          typename EqualKey = int >
class hash_set : public HASH_NAMESPACE::HASH_SET_CLASS<
    Key, HashFcn> {
};

#else

template <typename Key>
struct hash : public HASH_NAMESPACE::hash<Key> {
};

template <typename Key>
struct hash<const Key*> {
  inline size_t operator()(const Key* key) const {
    return reinterpret_cast<size_t>(key);
  }
};

template <>
struct hash<const char*> : public HASH_NAMESPACE::hash<const char*> {
};

template <typename Key, typename Data,
          typename HashFcn = hash<Key>,
          typename EqualKey = std::equal_to<Key> >
class hash_map : public HASH_NAMESPACE::HASH_MAP_CLASS<
    Key, Data, HashFcn, EqualKey> {
};

template <typename Key,
          typename HashFcn = hash<Key>,
          typename EqualKey = std::equal_to<Key> >
class hash_set : public HASH_NAMESPACE::HASH_SET_CLASS<
    Key, HashFcn, EqualKey> {
};

#endif

#if defined(REDEFINE_HASH_STRING)
template <>
struct hash<std::string> {
  inline size_t operator()(const std::string& key) const {
    return hash<const char*>()(key.c_str());
  }

  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
  inline size_t operator()(const std::string& a, const std::string& b) const {
    return a < b;
  }
};
#endif

template <typename First, typename Second>
struct hash<std::pair<First, Second> > {
  inline size_t operator()(const std::pair<First, Second>& key) const {
    size_t first_hash = hash<First>()(key.first);
    size_t second_hash = hash<Second>()(key.second);

    // FIXME(kenton):  What is the best way to compute this hash?  I have
    // no idea!  This seems a bit better than an XOR.
    return first_hash * ((1 << 16) - 1) + second_hash;
  }

  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
  inline size_t operator()(const std::pair<First, Second>& a,
                           const std::pair<First, Second>& b) const {
    return a < b;
  }
};

// Used by GCC/SGI STL only.  (Why isn't this provided by the standard
// library?  :( )
struct streq {
  inline bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) == 0;
  }
};

}  /* namespace drizzled */

#endif /* DRIZZLED_HASH_H */
