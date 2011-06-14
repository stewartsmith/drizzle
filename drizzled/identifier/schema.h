/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

/* 
  This is a "work in progress". The concept needs to be replicated throughout
  the code, but we will start with baby steps for the moment. To not incur
  cost until we are complete, for the moment it will do no allocation.

  This is mainly here so that it can be used in the SE interface for
  the time being.

  This will replace Table_ident.
  */

#pragma once

#include <boost/algorithm/string.hpp>
#include <ostream>

namespace drizzled {
namespace identifier {

class DRIZZLED_API Schema : public Identifier
{
  std::string db;
  std::string db_path;

public:
  Schema(const std::string &db_arg);

  virtual std::string getSQLPath() const
	{
		return db;
	}

  const std::string &getPath() const;

  const std::string &getSchemaName() const
  {
    return db;
  }

  const std::string &getCatalogName() const;

  virtual bool isValid() const;

  inline virtual bool isSystem() const
  {
    return false;
  }

  bool compare(const std::string &arg) const;
  bool compare(const Schema&) const;

  friend bool operator<(const Schema& left, const Schema& right)
  {
    return boost::ilexicographical_compare(left.getSchemaName(), right.getSchemaName());
  }

  friend bool operator==(const Schema& left,
                         const Schema& right)
  {
    return boost::iequals(left.getSchemaName(), right.getSchemaName());
  }
};

std::ostream& operator<<(std::ostream& output, const Schema&identifier);


} /* namespace identifier */
} /* namespace drizzled */
