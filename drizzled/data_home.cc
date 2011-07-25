/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
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
#include <boost/filesystem.hpp>
#include <drizzled/configmake.h>
#include <drizzled/data_home.h>

namespace drizzled {

static boost::filesystem::path data_home(LOCALSTATEDIR);
static boost::filesystem::path full_data_home(LOCALSTATEDIR);
static boost::filesystem::path data_home_catalog(LOCALSTATEDIR);

const boost::filesystem::path& getFullDataHome()
{
  return full_data_home;
}

const boost::filesystem::path& getDataHome()
{
  return data_home;
}

const boost::filesystem::path& getDataHomeCatalog()
{
  return data_home_catalog;
}

boost::filesystem::path& getMutableDataHome()
{
  return data_home;
}

void setFullDataHome(const boost::filesystem::path& v)
{
  full_data_home= v;
}

void setDataHomeCatalog(const boost::filesystem::path& v)
{
  data_home_catalog= v;
}

} // namespace drizzled
