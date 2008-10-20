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

#ifndef ERROR_INJECT_SUPPORT
/**  
 * @file 
 *
 * Simple compile-time error injection module to enable easy 
 * error printing in case of a crash
 */
#ifndef DRIZZLE_SERVER_ERROR_INJECTION_H
#define DRIZZLE_SERVER_ERROR_INJECTION_H

/*
  Error injector Macros to enable easy testing of recovery after failures
  in various error cases.
*/
#define ERROR_INJECT(x) 0
#define ERROR_INJECT_ACTION(x,action) 0
#define ERROR_INJECT_CRASH(x) 0
#define ERROR_INJECT_VALUE(x) 0
#define ERROR_INJECT_VALUE_ACTION(x,action) 0
#define ERROR_INJECT_VALUE_CRASH(x) 0
#define SET_ERROR_INJECT_VALUE(x)

#else

inline bool check_and_unset_keyword(const char *dbug_str)
{
  const char *extra_str= "-d,";
  char total_str[200];
  if (_db_strict_keyword_ (dbug_str))
  {
    strxmov(total_str, extra_str, dbug_str, NULL);
    return 1;
  }
  return 0;
}


inline bool
check_and_unset_inject_value(int value)
{
  Session *thd= current_thd;
  if (thd->error_inject_value == (uint)value)
  {
    thd->error_inject_value= 0;
    return 1;
  }
  return 0;
}

/*
  ERROR INJECT MODULE:
  --------------------
  These macros are used to insert macros from the application code.
  The event that activates those error injections can be activated
  from SQL by using:
  SET SESSION dbug=+d,code;

  After the error has been injected, the macros will automatically
  remove the debug code, thus similar to using:
  SET SESSION dbug=-d,code
  from SQL.

  ERROR_INJECT_CRASH will inject a crash of the MySQL Server if code
  is set when macro is called. ERROR_INJECT_CRASH can be used in
  if-statements, it will always return false unless of course it
  crashes in which case it doesn't return at all.

  ERROR_INJECT_ACTION will inject the action specified in the action
  parameter of the macro, before performing the action the code will
  be removed such that no more events occur. ERROR_INJECT_ACTION
  can also be used in if-statements and always returns FALSE.
  ERROR_INJECT can be used in a normal if-statement, where the action
  part is performed in the if-block. The macro returns TRUE if the
  error was activated and otherwise returns FALSE. If activated the
  code is removed.

  Sometimes it is necessary to perform error inject actions as a serie
  of events. In this case one can use one variable on the Session object.
  Thus one sets this value by using e.g. SET_ERROR_INJECT_VALUE(100).
  Then one can later test for it by using ERROR_INJECT_CRASH_VALUE,
  ERROR_INJECT_ACTION_VALUE and ERROR_INJECT_VALUE. This have the same
  behaviour as the above described macros except that they use the
  error inject value instead of a code used by debug macros.
*/
#define SET_ERROR_INJECT_VALUE(x) \
  current_thd->error_inject_value= (x)
#define ERROR_INJECT_ACTION(code, action) \
  (check_and_unset_keyword(code) ? ((action), 0) : 0)
#define ERROR_INJECT(code) \
  check_and_unset_keyword(code)
#define ERROR_INJECT_VALUE(value) \
  check_and_unset_inject_value(value)
#define ERROR_INJECT_VALUE_ACTION(value,action) \
  (check_and_unset_inject_value(value) ? (action) : 0)
#define ERROR_INJECT_VALUE_CRASH(value) \
  ERROR_INJECT_VALUE_ACTION(value, (abort(), 0))

#endif /* DRIZZLE_SERVER_ERROR_INJECTION_H */
#endif /* ERROR_INJECT_SUPPORT */
