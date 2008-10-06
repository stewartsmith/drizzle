/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

class DRIZZLE_ERROR: public Sql_alloc
{
public:
  enum enum_warning_level
  { WARN_LEVEL_NOTE, WARN_LEVEL_WARN, WARN_LEVEL_ERROR, WARN_LEVEL_END};

  uint32_t code;
  enum_warning_level level;
  char *msg;
  
  DRIZZLE_ERROR(THD *thd, uint32_t code_arg, enum_warning_level level_arg,
	      const char *msg_arg)
    :code(code_arg), level(level_arg)
  {
    if (msg_arg)
      set_msg(thd, msg_arg);
  }
  void set_msg(THD *thd, const char *msg_arg);
};

DRIZZLE_ERROR *push_warning(THD *thd, DRIZZLE_ERROR::enum_warning_level level,
                          uint32_t code, const char *msg);
void push_warning_printf(THD *thd, DRIZZLE_ERROR::enum_warning_level level,
			 uint32_t code, const char *format, ...);
void drizzle_reset_errors(THD *thd, bool force);
bool mysqld_show_warnings(THD *thd, uint32_t levels_to_show);

extern const LEX_STRING warning_level_names[];
