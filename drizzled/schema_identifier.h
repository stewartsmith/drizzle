/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef DRIZZLED_SCHEMA_IDENTIFIER_H
#define DRIZZLED_SCHEMA_IDENTIFIER_H

#include <drizzled/enum.h>
#include "drizzled/definitions.h"
#include "drizzled/message/table.pb.h"
#include <string.h>

#include <assert.h>

#include <ostream>
#include <list>
#include <algorithm>
#include <functional>
#include <iostream>

namespace drizzled {

class SchemaIdentifier
{
  std::string db;
  std::string db_path;
  std::string lower_db;

  // @note this should be changed to protected once Session contains an
  // identifier for current db.
public:

  const std::string &getLower()
  {
   return lower_db;
  }

public:
  SchemaIdentifier(const std::string &db_arg) :
    db(db_arg)
  { 
    lower_db.append(db_arg);

    std::transform(lower_db.begin(), lower_db.end(),
                   lower_db.begin(), ::tolower);
  }

  virtual ~SchemaIdentifier()
  { }

  virtual const std::string &getSQLPath();
  const std::string &getPath();

  const std::string &getSchemaName() const
  {
    return db;
  }

  bool isValid();
  bool compare(std::string arg);

  friend bool operator<(SchemaIdentifier &left, SchemaIdentifier &right)
  {
    return left.lower_db < right.lower_db;
  }

  friend std::ostream& operator<<(std::ostream& output, SchemaIdentifier &identifier)
  {
    output << "SchemaIdentifier:(";
    output <<  identifier.db;
    output << ", ";
    output << identifier.getPath();
    output << ")";

    return output;  // for multiple << operators.
  }

  friend bool operator==(SchemaIdentifier &left, SchemaIdentifier &right)
  {
    if (left.lower_db == right.lower_db)
    {
      return true;
    }

    return false;
  }

};

typedef std::list <SchemaIdentifier> SchemaIdentifierList;

} /* namespace drizzled */

#endif /* DRIZZLED_SCHEMA_IDENTIFIER_H */
