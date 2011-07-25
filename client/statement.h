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

#ifndef CLIENT_STATEMENT_H
#define CLIENT_STATEMENT_H

#include "client_priv.h"
#include <string>
#include <iostream>
#include <cstdlib>


/* Types */
enum slap_query_t {
  SELECT_TYPE= 0,
  UPDATE_TYPE= 1,
  INSERT_TYPE= 2,
  UPDATE_TYPE_REQUIRES_PREFIX= 3,
  CREATE_TABLE_TYPE= 4,
  SELECT_TYPE_REQUIRES_PREFIX= 5,
  DELETE_TYPE_REQUIRES_PREFIX= 6
};


class Statement 
{
public:
  Statement(char *in_string,
            size_t in_length,
            slap_query_t in_type,
            Statement *in_next) :
    string(in_string),
    length(in_length),
    type(in_type),
    next(in_next)
  { }

  Statement() :
    string(NULL),
    length(0),
    type(),
    next(NULL)
  { }

  ~Statement()
  {
    free(string);
  }
   
  char *getString() const
  {
    return string;
  }

  size_t getLength() const
  {
    return length;
  }

  slap_query_t getType() const
  {
    return type;
  }

  Statement *getNext() const
  {
    return next;
  }

  void setString(char *in_string)
  {
    string= in_string;
  }

  void setString(size_t length_arg)
  {
    string= (char *)calloc(length_arg + 1, sizeof(char));
    length= length_arg;
  }

  void setString(size_t in_length, char in_char)
  {
    string[in_length]= in_char;
  }

  void setLength(size_t in_length)
  {
    length= in_length;
  }

  void setType(slap_query_t in_type)
  {
    type= in_type;
  }

  void setNext(Statement *in_next)
  {
    next= in_next;
  }

private:
  char *string;
  size_t length;
  slap_query_t type;
  Statement *next;
};

#endif /* CLIENT_STATEMENT_H */
