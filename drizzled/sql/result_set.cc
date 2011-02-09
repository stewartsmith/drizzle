/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2011, Brian Aker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of Patrick Galbraith nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <drizzled/sql/exception.h>
#include <drizzled/sql/result_set.h>

#include <iostream>

namespace drizzled {
namespace sql {

static Exception exception_unknown_column("Unknown Column", "S0022", ER_BAD_FIELD_ERROR);
static Exception exception_no_more_results("No additional rows founds", "S0022", ER_BAD_FIELD_ERROR);

ResultSet::~ResultSet()
{
}

const std::string ResultSet::getString(size_t column_number) const
{
  if (not isMore(column_number))
    return "";

  return (*_current_row)[column_number].value();
}

bool ResultSet::isNull(size_t column_number) const
{
  return (*_current_row)[column_number].isNull();
}

void ResultSet::pushException(const Exception &arg) const
{
  if (_exceptions.empty())
  {
    _exceptions.push(arg);
    return;
  }

  _exceptions.front().setNextException(arg);
}

bool ResultSet::isMore() const
{
  if (_current_row == _results.end())
  {
    pushException(exception_no_more_results);
    return false;
  }

  return true;
}

bool ResultSet::isMore(size_t column_number) const
{
  if (column_number >= _meta_data.getColumnCount())
  {
    pushException(exception_unknown_column);

    return false;
  }

  return isMore();
}

bool ResultSet::error() const
{
  return not _exceptions.empty();
}

sql::Exception ResultSet::getException() const
{
  return _exceptions.empty() ? sql::Exception() : _exceptions.front();
}

const ResultSetMetaData &ResultSet::getMetaData() const
{
  return _meta_data;
}

void ResultSet::createRow()
{
  assert(_meta_data.getColumnCount());
  _results.resize(_results.size() +1);
  _results.back().resize(_meta_data.getColumnCount());
}

void ResultSet::setColumn(size_t column_number, const std::string &arg)
{
  assert(column_number < _meta_data.getColumnCount());
  assert(_results.back().at(column_number).isNull() == false); // ie the default value
  assert(_results.back().at(column_number).value().empty() == true); // ie no value has been set yet
  _results.back().at(column_number).set_value(arg);
}

void ResultSet::setColumnNull(size_t column_number)
{
  assert(column_number < _meta_data.getColumnCount());
  assert(_results.back().at(column_number).isNull() == false); // ie the default value
  assert(_results.back().at(column_number).value().empty() == true); // ie no value has been set yet
  _results.back().at(column_number).set_null();
}

bool ResultSet::next() const
{
  if (not _has_next_been_called)
  {
    _current_row= _results.begin();
    _has_next_been_called= true;
  }
  else
  {
    _current_row++;
  }

  if (_current_row == _results.end())
    return false;

  return true;
}

std::ostream& operator<<(std::ostream& output, const ResultSet &result_set)
{
  while (result_set.next())
  {
    for (size_t x= 0; x < result_set.getMetaData().getColumnCount(); x++)
    {
      if (result_set.isNull(x))
      {
        output << "<null>" << '\t';
      }
      else 
      {
        output << result_set.getString(x) << '\t';
      }
    }
    output << std::endl;
  }

  return output;
}

} // namespace sql 
} // namespace drizzled
