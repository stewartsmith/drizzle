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


#ifndef DRIZZLED_ERROR_H
#define DRIZZLED_ERROR_H

#include <string>
#include <boost/unordered_map.hpp>

#include "drizzled/error_t.h"
#include "drizzled/definitions.h"
#include "drizzled/identifier.h"

#include "drizzled/visibility.h"

namespace drizzled
{

/* Max width of screen (for error messages) */
#define SC_MAXWIDTH 256
#define ERRMSGSIZE	(SC_MAXWIDTH)	/* Max length of a error message */
#define MY_FILE_ERROR	((size_t) -1)
#define ME_FATALERROR   1024    /* Fatal statement error */

/*
 * Provides a mapping from the error enum values to std::strings.
 */
class ErrorMap
{
public:
  typedef std::pair<std::string, std::string> value_type;
  typedef boost::unordered_map<drizzled::error_t, value_type> ErrorMessageMap;

  ErrorMap();

  // Insert the message for the error.  If the error already has an existing
  // mapping, an error is logged, but the function continues.
  void add(drizzled::error_t error_num, const std::string &error_name, const std::string &message);

  // If there is no error mapping for the error_num, ErrorStringNotFound is raised.
  const std::string &find(drizzled::error_t error_num) const;

  static const ErrorMessageMap& get_error_message_map();
private:
  // Disable copy and assignment.
  ErrorMap(const ErrorMap &e);
  ErrorMap& operator=(const ErrorMap &e);

  ErrorMessageMap mapping_;
};


typedef void (*error_handler_func)(drizzled::error_t my_err,
                                   const char *str,
                                   myf MyFlags);
extern error_handler_func error_handler_hook;

// TODO: kill this method. Too much to do with this branch.
// This is called through the ER(x) macro.
DRIZZLED_API const char * error_message(drizzled::error_t err_index);

// Adds the message to the global error dictionary.
void add_error_message(drizzled::error_t error_code, const std::string &error_name,
                       const std::string& message);
#define DRIZZLE_ADD_ERROR_MESSAGE(code, msg) add_error_message(code, STRINGIFY_ARG(code), msg)

DRIZZLED_API void my_error(const std::string &ref, error_t nr, myf MyFlags= MYF(0));
DRIZZLED_API void my_error(error_t nr, drizzled::Identifier::const_reference ref, myf MyFlags= MYF(0));
DRIZZLED_API void my_error(error_t nr);
DRIZZLED_API void my_error(error_t nr, myf MyFlags, ...);
void my_message(drizzled::error_t my_err, const char *str, myf MyFlags);
void my_printf_error(drizzled::error_t my_err, const char *format,
                     myf MyFlags, ...)
                     __attribute__((format(printf, 2, 4)));

} /* namespace drizzled */

#endif /* DRIZZLED_ERROR_H */
