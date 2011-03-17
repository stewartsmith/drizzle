/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#pragma once

/**
 * @file Defines a Plugin Library Wrapper
 *
 * A plugin::Library is a wrapper object around a plugin .so file. It owns
 * the void * returned by dlopen and contains the knowledge of how to load
 * and unload plugin libraries via dlopen().
 */

#include <string>

#include <boost/filesystem.hpp>

namespace drizzled
{
namespace module
{

struct Manifest;

/* A handle for the dynamic library containing a plugin or plugins. */
class Library
{
  std::string name;
  void *handle;
  const Manifest *manifest;

  /* We don't want these */
  Library();
  Library(const Library &);
  Library& operator=(const Library &);

  /* Construction should only happen through the static factory method */
  Library(const std::string &name_arg,
          void *handle_arg,
          const Manifest *manifest_arg);

public:
  ~Library();

  const std::string &getName() const
  {
    return name;
  }
 
  const Manifest *getManifest() const
  {
    return manifest;
  }

  static const boost::filesystem::path getLibraryPath(const std::string &plugin_name);
  static Library *loadLibrary(const std::string &plugin_name, bool builtin);
};

} /* namespace module */
} /* namespace drizzled */

