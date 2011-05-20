/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Drizzle Execute Parser
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following disclaimer
 *  in the documentation and/or other materials provided with the
 *  distribution.
 *
 *      * The names of its contributors may not be used to endorse or
 *  promote products derived from this software without specific prior
 *  written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include <config.h>
#include <cstdlib>
#include <vector>
#include <string>
#include <drizzled/error_t.h>


namespace drizzled {
namespace execute {

class Context
{
public:
  Context(const char *option_string, size_t option_string_length, drizzled::error_t &rc_arg) :
    scanner(NULL),
    begin(NULL),
    pos(0),
    rc(rc_arg),
    _is_server(false),
    _end(false)
  {
    buf= option_string;
    length= option_string_length;
    init_scanner();
    rc= EE_OK;
  }

  bool end()
  {
    return _end;
  }

  std::vector<std::string> start();

  void set_end()
  {
    rc= EE_OK;
    _end= true;
  }

  ~Context()
  {
    destroy_scanner();
  }

  void *scanner;
  const char *buf;
  const char *begin;
  size_t pos;
  size_t length;
  drizzled::error_t &rc;
  bool _is_server;

protected:
  void init_scanner();   
  void destroy_scanner();

private:
  bool _end;
}; 

} // namespace execute
} // namespace drizzled
