/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <drizzled/select_result_interceptor.h>
#include <drizzled/file_exchange.h>

#include <boost/filesystem.hpp>

namespace drizzled
{

namespace internal
{
struct st_io_cache;
}

class select_to_file :
  public select_result_interceptor
{
protected:
  file_exchange *exchange;
  int file;
  internal::io_cache_st *cache;
  ha_rows row_count;
  boost::filesystem::path path;

public:
  select_to_file(file_exchange *ex);
  virtual ~select_to_file();
  void send_error(drizzled::error_t errcode,const char *err);
  bool send_eof();
  void cleanup();
};

} /* namespace drizzled */

