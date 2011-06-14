/* Copyright (C) 2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#pragma once

#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

#include <libgearman/gearman.h>

class Item_func_gman_do :public drizzled::Item_str_func
{
protected:
  typedef enum
  {
    GMAN_DO_OPTIONS_NONE=       0,
    GMAN_DO_OPTIONS_HIGH=       (1 << 0),
    GMAN_DO_OPTIONS_LOW=        (1 << 1),
    GMAN_DO_OPTIONS_BACKGROUND= (1 << 2),
    GMAN_DO_OPTIONS_CLIENT=     (1 << 3)
  } gman_do_options_t;

private:
  gman_do_options_t options;
  gearman_client_st client;
  drizzled::String buffer;

public:
  Item_func_gman_do():
    Item_str_func(),
    options(GMAN_DO_OPTIONS_NONE) {}
  Item_func_gman_do(gman_do_options_t options_arg):
    Item_str_func(),
    options(options_arg) {}
  ~Item_func_gman_do();
  void fix_length_and_dec() { max_length=10; }
  virtual const char *func_name() const{ return "gman_do"; }
  drizzled::String *val_str(drizzled::String *);
  void *realloc(size_t size);
};

class Item_func_gman_do_high :public Item_func_gman_do
{
public:
  Item_func_gman_do_high():
    Item_func_gman_do(GMAN_DO_OPTIONS_HIGH) {}
  const char *func_name() const{ return "gman_do_high"; }
};

class Item_func_gman_do_low :public Item_func_gman_do
{
public:
  Item_func_gman_do_low():
    Item_func_gman_do(GMAN_DO_OPTIONS_LOW) {}
  const char *func_name() const{ return "gman_do_low"; }
};

class Item_func_gman_do_background :public Item_func_gman_do
{
public:
  Item_func_gman_do_background():
    Item_func_gman_do(GMAN_DO_OPTIONS_BACKGROUND) {}
  const char *func_name() const{ return "gman_do_background"; }
};

class Item_func_gman_do_high_background :public Item_func_gman_do
{
public:
  Item_func_gman_do_high_background():
    Item_func_gman_do((gman_do_options_t)(GMAN_DO_OPTIONS_HIGH |
                                          GMAN_DO_OPTIONS_BACKGROUND)) {}
  const char *func_name() const{ return "gman_do_high_background"; }
};

class Item_func_gman_do_low_background :public Item_func_gman_do
{
public:
  Item_func_gman_do_low_background():
    Item_func_gman_do((gman_do_options_t)(GMAN_DO_OPTIONS_LOW |
                                         GMAN_DO_OPTIONS_BACKGROUND)) {}
  const char *func_name() const{ return "gman_do_low_background"; }
};

