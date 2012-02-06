/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Andrew Hutchings
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License, or
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

#include "client_priv.h"

#include <boost/regex.hpp>
#include <iostream>
#include <client/server_detect.h>

ServerDetect::ServerDetect(drizzle_con_st *connection) :
  type(SERVER_UNKNOWN_FOUND),
  version("")
{
  boost::match_flag_type flags = boost::match_default;

  // FIXME: Detecting capabilities from a version number is a recipe for
  // disaster, like we've seen with 15 years of JavaScript :-)
  // Anyway, as there is no MySQL 7.x yet, this will do for tonight.
  // I will get back to detect something tangible after the release (like
  // presence of some table or its record in DATA_DICTIONARY.
  boost::regex mysql_regex("^([3-6]\\.[0-9]+\\.[0-9]+)");
  boost::regex drizzle_regex7("^(20[0-9]{2}\\.(0[1-9]|1[012])\\.[0-9]+)");
  boost::regex drizzle_regex71("^([7-9]\\.[0-9]+\\.[0-9]+)");

  version= drizzle_con_server_version(connection);

  if (regex_search(version, drizzle_regex7, flags))
  {
    type= SERVER_DRIZZLE_FOUND;
  }
  else if (regex_search(version, drizzle_regex71, flags))
  {
    type= SERVER_DRIZZLE_FOUND;
  }
  else if (regex_search(version, mysql_regex, flags))
  {
    type= SERVER_MYSQL_FOUND;
  }
  else
  {
    std::cerr << "Server version not detectable. Assuming MySQL." << std::endl;
    type= SERVER_MYSQL_FOUND;
  }
}
