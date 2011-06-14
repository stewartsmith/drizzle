/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <bitset>
#include <drizzled/error_t.h>
#include <drizzled/lex_string.h>
#include <drizzled/memory/sql_alloc.h>
#include <drizzled/visibility.h>

namespace drizzled {

class DRIZZLE_ERROR : public memory::SqlAlloc
{
public:
  static const uint32_t NUM_ERRORS= 4;
  enum enum_warning_level {
    WARN_LEVEL_NOTE,
    WARN_LEVEL_WARN,
    WARN_LEVEL_ERROR,
    WARN_LEVEL_END
  };

  drizzled::error_t code;
  enum_warning_level level;
  char *msg;

  DRIZZLE_ERROR(Session *session,
                drizzled::error_t code_arg,
                enum_warning_level level_arg,
                const char *msg_arg) :
    code(code_arg),
    level(level_arg)
  {
    if (msg_arg)
      set_msg(session, msg_arg);
  }

  void set_msg(Session *session, const char *msg_arg);
};

DRIZZLED_API DRIZZLE_ERROR *push_warning(Session *session, DRIZZLE_ERROR::enum_warning_level level,
                            drizzled::error_t code, const char *msg);

DRIZZLED_API void push_warning_printf(Session *session, DRIZZLE_ERROR::enum_warning_level level,
                         drizzled::error_t code, const char *format, ...);

void drizzle_reset_errors(Session *session, bool force);
bool show_warnings(Session *session, 
                   std::bitset<DRIZZLE_ERROR::NUM_ERRORS> &levels_to_show);

extern const LEX_STRING warning_level_names[];

} /* namespace drizzled */

