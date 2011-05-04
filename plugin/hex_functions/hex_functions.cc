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

#include <config.h>

#include <drizzled/charset.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/plugin/function.h>
#include <drizzled/util/convert.h>

using namespace drizzled;

class HexFunction :public Item_str_func
{
  String tmp_value;
public:
  HexFunction() :Item_str_func() {}
  const char *func_name() const { return "hex"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(default_charset());
    decimals=0;
    max_length=args[0]->max_length*2*collation.collation->mbmaxlen;
  }

  bool check_argument_count(int n) { return n == 1; }
};

class UnHexFunction :public Item_str_func
{
  String tmp_value;
public:
  UnHexFunction() :Item_str_func()
  {
    /* there can be bad hex strings */
    maybe_null= 1;
  }
  const char *func_name() const { return "unhex"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=(1+args[0]->max_length)/2;
  }
  bool check_argument_count(int n) { return n == 1; }
};

/**
  convert a hex digit into number.
*/
static int hexchar_to_int(char c)
{
  if (c <= '9' && c >= '0')
    return c-'0';
  c|=32;
  if (c <= 'f' && c >= 'a')
    return c-'a'+10;
  return -1;
}

String *HexFunction::val_str(String *str)
{
  String *res;
  assert(fixed == 1);
  if (args[0]->result_type() != STRING_RESULT)
  {
    uint64_t dec;
    char ans[65],*ptr;
    /* Return hex of unsigned int64_t value */
    if (args[0]->result_type() == REAL_RESULT ||
        args[0]->result_type() == DECIMAL_RESULT)
    {
      double val= args[0]->val_real();
      if ((val <= (double) INT64_MIN) ||
          (val >= (double) (uint64_t) UINT64_MAX))
        dec=  ~(int64_t) 0;
      else
        dec= (uint64_t) (val + (val > 0 ? 0.5 : -0.5));
    }
    else
      dec= (uint64_t) args[0]->val_int();

    if ((null_value= args[0]->null_value))
      return 0;
    ptr= internal::int64_t2str(dec,ans,16);
    str->copy(ans,(uint32_t) (ptr-ans),default_charset());
    return str;
  }

  /* Convert given string to a hex string, character by character */
  res= args[0]->val_str(str);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  tmp_value.alloc(res->length()*2+1);
  tmp_value.length(res->length()*2);

  (void) drizzled_string_to_hex((char*) tmp_value.ptr(), res->ptr(),
                                res->length());
  return &tmp_value;
}

  /** Convert given hex string to a binary string. */

String *UnHexFunction::val_str(String *str)
{
  const char *from, *end;
  char *to;
  String *res;
  uint32_t length;
  assert(fixed == 1);

  res= args[0]->val_str(str);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  tmp_value.alloc(length= (1+res->length())/2);

  from= res->ptr();
  null_value= 0;
  tmp_value.length(length);
  to= (char*) tmp_value.ptr();
  if (res->length() % 2)
  {
    int hex_char;
    *to++= hex_char= hexchar_to_int(*from++);
    if ((null_value= (hex_char == -1)))
      return 0;
  }
  for (end=res->ptr()+res->length(); from < end ; from+=2, to++)
  {
    int hex_char;
    *to= (hex_char= hexchar_to_int(from[0])) << 4;
    if ((null_value= (hex_char == -1)))
      return 0;
    *to|= hex_char= hexchar_to_int(from[1]);
    if ((null_value= (hex_char == -1)))
      return 0;
  }
  return &tmp_value;
}

plugin::Create_function<HexFunction> *hex_function= NULL;
plugin::Create_function<UnHexFunction> *unhex_function= NULL;

static int initialize(drizzled::module::Context &context)
{
  hex_function= new plugin::Create_function<HexFunction>("hex");
  unhex_function= new plugin::Create_function<UnHexFunction>("unhex");
  context.add(hex_function);
  context.add(unhex_function);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "hex_functions",
  "1.0",
  "Stewart Smith",
  "Convert a string to HEX() or from UNHEX()",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
