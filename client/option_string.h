/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Vijay Samuel
 *  Copyright (C) 2008 MySQL
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

#ifndef CLIENT_OPTION_STRING_H
#define CLIENT_OPTION_STRING_H

#include "client_priv.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <drizzled/gettext.h>

class OptionString 
{
public:
  OptionString(char *in_string,
               size_t in_length,
               char *in_option,
               size_t in_option_length,
               OptionString *in_next) :
    string(in_string),
    length(in_length),
    option(in_option),
    option_length(in_option_length),
    next(in_next)
  { }  

  OptionString() :
    string(NULL),
    length(0),
    option(NULL),
    option_length(0),
    next(NULL)
  { }

  ~OptionString()
  {
    if (getString())
      free(getString());
    if (getOption())
      free(getOption());
  }

  char *getString() const
  {
    return string;
  }

  size_t getLength() const
  {
    return length;
  }

  char *getOption() const
  {
  return option;
  }

  size_t getOptionLength() const
  {
    return option_length;
  }

  OptionString *getNext() const
  {
    return next;
  }

  void setString(char *in_string)
  {
    string= in_string;
    length= strlen(in_string);
  }

  void setOption(char *in_option)
  {
    option= strdup(in_option);
    option_length= strlen(in_option);
  }

  void setNext(OptionString *in_next)
  {
    next= in_next;
  }
  
private:
  char *string;
  size_t length;
  char *option;
  size_t option_length;
  OptionString *next;
};

#endif /* CLIENT_OPTION_STRING_H */
