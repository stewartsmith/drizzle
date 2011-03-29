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

#include <drizzled/error_t.h>

#include <queue>
#include <stdexcept>

namespace drizzled {
namespace sql {

class Exception :  public std::runtime_error
{
public:
  Exception(std::string reason, std::string sql_state, drizzled::error_t error_arg) :
    std::runtime_error(reason),
    _error_code(error_arg),
    _sql_state(sql_state),
    _reason(reason)
  {
    if (_sql_state.length() != 5)
      _sql_state= "HY000";
  }

  Exception() :
    std::runtime_error("no error"),
    _error_code(drizzled::EE_OK),
    _sql_state("00000")
  {
  }

  Exception(std::string reason, drizzled::error_t error_arg) :
    std::runtime_error(reason),
    _error_code(error_arg),
    _reason(reason)
  {
    _sql_state= "00000";
  }

  ~Exception () throw ()
  { }

  drizzled::error_t getErrorCode() const
  {
    return _error_code;
  }

  const std::string &getErrorMessage() const
  {
    return _reason;
  }

  Exception getNextException() const
  {
    if (_next_exception.empty())
    {
      return Exception();
    }

    Exception tmp= _next_exception.front();
    _next_exception.pop();

    return Exception();
  }

  const std::string &getSQLState() const
  {
    return _sql_state;
  }

protected:
  friend class ResultSet;
  void setNextException(Exception arg) 
  {
    (void)arg;
    return;
  }

private:
  drizzled::error_t _error_code; // Vendor, ie our, code
  std::string _sql_state;
  std::string _reason;
  mutable std::queue <Exception> _next_exception;
};

std::ostream& operator<<(std::ostream& output, const Exception &arg);

} // namespace sql 
} // namespace drizzled

