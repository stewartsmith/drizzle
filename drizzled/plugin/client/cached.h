/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#pragma once

#include <boost/lexical_cast.hpp>
#include <drizzled/field.h>
#include <drizzled/plugin/client/concurrent.h>
#include <drizzled/sql/result_set.h>
#include <iostream>

namespace drizzled {
namespace plugin {
namespace client {

class Cached : public Concurrent
{
  uint32_t column;
  uint32_t max_column;
  sql::ResultSet *_result_set;

public:
  Cached(sql::ResultSet &rs) :
    column(0),
    max_column(0),
    _result_set(&rs)
  {
  }

  virtual void sendFields(List<Item>& list)
  {
    List<Item>::iterator it(list.begin());

    column= 0;
    max_column= 0;

    while (Item* item= it++)
    {
      SendField field;
      item->make_field(&field);
      max_column++;
    }
    _result_set->setColumnCount(max_column);
    _result_set->createRow();
  }

  virtual void sendError(drizzled::error_t error_code, const char *error_message)
  {
    _result_set->pushException(sql::Exception(error_message, error_code));
  }

  virtual void checkRowEnd()
  {
    if (++column % max_column == 0)
    {
      _result_set->createRow();
    }
  }

  using Client::store;

  virtual void store(Field *from)
  {
    if (from->is_null())
      return store();

    char buff[MAX_FIELD_WIDTH];
    String str(buff, sizeof(buff), &my_charset_bin);
    from->val_str_internal(&str);

    return store(str.ptr(), str.length());
  }

  virtual void store()
  {
    _result_set->setColumnNull(currentColumn());
    checkRowEnd();
  }

  virtual void store(int32_t from)
  {
    _result_set->setColumn(currentColumn(), boost::lexical_cast<std::string>(from));
    checkRowEnd();
  }

  virtual void store(uint32_t from)
  {
    _result_set->setColumn(currentColumn(), boost::lexical_cast<std::string>(from));
    checkRowEnd();
  }

  virtual void store(int64_t from)
  {
    _result_set->setColumn(currentColumn(), boost::lexical_cast<std::string>(from));
    checkRowEnd();
  }

  virtual void store(uint64_t from)
  {
    _result_set->setColumn(currentColumn(), boost::lexical_cast<std::string>(from));
    checkRowEnd();
  }

  virtual void store(double from, uint32_t decimals, String *buffer)
  {
    buffer->set_real(from, decimals, &my_charset_bin);
    return store(buffer->ptr(), buffer->length());
  }

  virtual void store(const char *from, size_t length)
  {
    _result_set->setColumn(currentColumn(), std::string(from, length));
    checkRowEnd();
  }
  
  inline uint32_t currentColumn() const
  {
    return column % max_column;
  }
};

} /* namespace client */
} /* namespace plugin */
} /* namespace drizzled */

