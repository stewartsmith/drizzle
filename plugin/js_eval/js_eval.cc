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

#include <drizzled/plugin/function.h>
#include <drizzled/function/str/strfunc.h>

using namespace std;
using namespace drizzled;

// TODO: So this is a function that returns strings? What is the class that returns mixed types?

class JsEvalFunction :public Item_str_func
{
public:
  JsEvalFunction() :Item_str_func() {}
  ~JsEvalFunction() {}

  String *val_str(String *);

  const char *func_name() const 
  { 
    return "js_eval"; 
  }

  void fix_length_and_dec() 
  { 
    maybe_null= 1;
    max_length= MAX_BLOB_WIDTH;   
  }

  bool check_argument_count(int n)
  {
    return (n >= 1);
  }
};


String *JsEvalFunction::val_str(String *str)
{
  assert(fixed == 1);
  
  str = args[0]->val_str(str);

  for( uint64_t n = 1; n < arg_count; n++ )
  {
    //TODO: collect other arguments into some array passed into v8 as js array "argv"
    //... = args[n];
  }
  
 
  // TODO: Call into v8...

  if (str == NULL)
  {
    null_value= true;
    return 0;
  }
                                   
  null_value= false;
  return str; // TODO: pass thru for now
}

plugin::Create_function<JsEvalFunction> *js_eval_function = NULL;

static int initialize(module::Context &context)
{
  js_eval_function = new plugin::Create_function<JsEvalFunction>("js_eval");
  context.add(js_eval_function);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "js_eval",
  "0.1",
  "Henrik Ingo",
  "Execute JavaScript code with supplied arguments",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* depends */              
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
       