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

namespace drizzle_plugin {
namespace syslog {
namespace logging {

class Syslog: public drizzled::plugin::Logging
{
private:
  int _facility;
  std::string sysvar_facility;

public:
  Syslog(const std::string &facility,
         drizzled::uint64_constraint threshold_slow,
         drizzled::uint64_constraint threshold_big_resultset,
         drizzled::uint64_constraint threshold_big_examined);
  
  /*
  These variables are made public as, otherwise, we will have to make setter functions for each of these variables to change their value 
  at runtime or we will have to make these variables extern. Changing them to public ensures that they can be changed at runtime directly.
  */
  drizzled::uint64_constraint _threshold_slow;
  drizzled::uint64_constraint _threshold_big_resultset;
  drizzled::uint64_constraint _threshold_big_examined;
  virtual bool post (drizzled::Session *session);
  bool setFacility(std::string new_facility);
  std::string& getFacility();
};

} /* namespace logging */
} /* namespace syslog */
} /* namespace drizzle_plugin */

