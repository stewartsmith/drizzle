/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Mark Atwood
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

#include <drizzled/plugin/logging.h>

namespace drizzle_plugin
{
namespace logging
{

class Syslog: public drizzled::plugin::Logging
{
private:
  int _facility;
  int _priority;
  uint64_t _threshold_slow;
  uint64_t _threshold_big_resultset;
  uint64_t _threshold_big_examined;

  Syslog();
  Syslog(const Syslog&);
  Syslog& operator=(const Syslog&);

public:
  Syslog(const std::string &facility,
         const std::string &priority,
         uint64_t threshold_slow,
         uint64_t threshold_big_resultset,
         uint64_t threshold_big_examined);

  virtual bool post (drizzled::Session *session);
};

} /* namespace logging */
} /* namespace drizzle_plugin */

