/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <boost/foreach.hpp>
#include <drizzled/display.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <iostream>
#include <fstream>
#include <string>

#include <drizzled/data_home.h>
#include <drizzled/cached_directory.h>
#include <drizzled/catalog/local.h>
#include <plugin/catalog/module.h>

namespace plugin {
namespace catalog {

static std::string CATALOG_OPT_EXT(".cat");

bool Engine::create(const drizzled::identifier::Catalog &identifier, drizzled::message::catalog::shared_ptr &message)
{
  if (mkdir(identifier.getPath().c_str(), 0777) == -1)
    return false;

  if (not writeFile(identifier, message))
  {
    rmdir(identifier.getPath().c_str());

    return false;
  }

  return true;
}

bool Engine::drop(const drizzled::identifier::Catalog &identifier)
{
  std::string file(identifier.getPath());
  file.append(1, FN_LIBCHAR);
  file.append(CATALOG_OPT_EXT);

  // No catalog file, no love from us.
  if (access(file.c_str(), F_OK))
  {
    perror(file.c_str());
    return false;
  }

  if (unlink(file.c_str()))
  {
    perror(file.c_str());
    return false;
  }

  if (rmdir(identifier.getPath().c_str()))
  {
    perror(identifier.getPath().c_str());
    //@todo If this happens, we want a report of it. For the moment I dump
    //to stderr so I can catch it in Hudson.
    drizzled::CachedDirectory dir(identifier.getPath());
  }

  return true;
}

void Engine::getMessages(drizzled::message::catalog::vector &messages)
{
  prime(messages);
}

drizzled::message::catalog::shared_ptr Engine::getMessage(const drizzled::identifier::Catalog& identifier)
{
  if (drizzled::catalog::local_identifier() == identifier)
  {
    return drizzled::message::catalog::make_shared(identifier);
  }

  drizzled::message::catalog::shared_ptr message;
  if ((message= readFile(identifier)))
  {
    assert(message);
    return message;
  }

  return drizzled::message::catalog::shared_ptr();
}

void Engine::prime(drizzled::message::catalog::vector &messages)
{
  bool found_local= false;
  drizzled::CachedDirectory directory(drizzled::getFullDataHome().file_string(), drizzled::CachedDirectory::DIRECTORY, true);
  drizzled::CachedDirectory::Entries files= directory.getEntries();


  BOOST_FOREACH(drizzled::CachedDirectory::Entries::reference entry, files)
  {
    drizzled::message::catalog::shared_ptr message;

    if (not entry->filename.compare(GLOBAL_TEMPORARY_EXT))
      continue;

    drizzled::identifier::Catalog identifier(entry->filename);

    if (message= readFile(identifier))
    {
      messages.push_back(message);

      if (drizzled::catalog::local_identifier() == identifier)
        found_local= true;
    }
  }

  if (not found_local)
  {
    messages.push_back(drizzled::catalog::local()->message());
  }
}

bool Engine::writeFile(const drizzled::identifier::Catalog &identifier, drizzled::message::catalog::shared_ptr &message)
{
  char file_tmp[FN_REFLEN];
  std::string file(identifier.getPath());


  file.append(1, FN_LIBCHAR);
  file.append(CATALOG_OPT_EXT);

  snprintf(file_tmp, FN_REFLEN, "%sXXXXXX", file.c_str());

  int fd= mkstemp(file_tmp);

  if (fd == -1)
  {
    perror(file_tmp);

    return false;
  }

  bool success;

  try {
    success= message->SerializeToFileDescriptor(fd);
  }
  catch (...)
  {
    success= false;
  }

  if (not success)
  {
    drizzled::my_error(drizzled::ER_CORRUPT_CATALOG_DEFINITION, MYF(0), file.c_str(),
                       message->InitializationErrorString().empty() ? "unknown" :  message->InitializationErrorString().c_str());

    if (close(fd) == -1)
      perror(file_tmp);

    if (unlink(file_tmp))
      perror(file_tmp);

    return false;
  }

  if (close(fd) == -1)
  {
    perror(file_tmp);

    if (unlink(file_tmp))
      perror(file_tmp);

    return false;
  }

  if (rename(file_tmp, file.c_str()) == -1)
  {
    if (unlink(file_tmp))
      perror(file_tmp);

    return false;
  }

  return true;
}


drizzled::message::catalog::shared_ptr Engine::readFile(const drizzled::identifier::Catalog& identifier)
{
  std::string path(identifier.getPath());

  /*
    Pass an empty file name, and the database options file name as extension
    to avoid table name to file name encoding.
  */
  path.append(1, FN_LIBCHAR);
  path.append(CATALOG_OPT_EXT);

  std::fstream input(path.c_str(), std::ios::in | std::ios::binary);

  if (input.good())
  {
    drizzled::message::catalog::shared_ptr message= drizzled::message::catalog::make_shared(identifier);

    if (not message)
      return drizzled::message::catalog::shared_ptr();


    if (message->ParseFromIstream(&input))
    {
      return message;
    }

    drizzled::my_error(drizzled::ER_CORRUPT_CATALOG_DEFINITION, MYF(0), path.c_str(),
                       message->InitializationErrorString().empty() ? "unknown" :  message->InitializationErrorString().c_str());
  }
  else
  {
    perror(path.c_str());
  }

  return drizzled::message::catalog::shared_ptr();
}

} /* namespace catalog */
} /* namespace plugin */
