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

#pragma once

#include <drizzled/visibility.h>
#include <drizzled/sql/exception.h>
#include <drizzled/sql/result_set_meta_data.h>
#include <cassert>
#include <queue>

namespace drizzled {
namespace sql {

class DRIZZLED_API ResultSet
{
  // First version of API stores everything as strings
  class Column {
  public:
    Column() :
      _is_null(false)
    { }

    inline const std::string &value() const
    {
      return _value;
    }

    inline bool isNull() const
    {
      return _is_null;
    }

    inline void set_value(const std::string &ref)
    {
      _value= ref;
    }

    inline void set_null()
    {
      assert(_value.empty());
      _is_null= true;
    }

  private:
    std::string _value;
    bool _is_null;
  };

  typedef std::vector< Column > Row;
  typedef std::vector< Row > Result;

public:
  static ResultSet *make(size_t field_count)
  {
    return new ResultSet(field_count);
  }

  bool next() const;

  const std::string getString(size_t column_number) const ;
  bool isNull(size_t column_number) const;
  const ResultSetMetaData &getMetaData() const;

  // Our functions to use instead of exceptions
  bool error() const;
  sql::Exception getException() const;

  ResultSet(size_t fields) :
    _has_next_been_called(false),
    _current_row(_results.end()),
    _meta_data(fields)
  {
  }

  void setColumnCount(size_t fields)
  {
    _meta_data.setColumnCount(fields);
  }

  ~ResultSet();

  void createRow();
  void setColumn(size_t column_number, const std::string &arg);
  void setColumnNull(size_t column_number);
  void pushException(const Exception &arg) const;

private: // Member methods
  bool isMore() const;
  bool isMore(size_t column_number) const;

private: // Member variables
  mutable bool _has_next_been_called;
  Result _results;
  mutable Result::const_iterator _current_row;
  ResultSetMetaData _meta_data;
  
  // Because an error could come up during a fetch on const, we need to have
  // this be mutable.
  mutable std::queue<Exception> _exceptions;
};

std::ostream& operator<<(std::ostream& output, const ResultSet &result_set);

} // namespace sql 
} // namespace drizzled

