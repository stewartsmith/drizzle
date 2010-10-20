/*
  Copyright (C) 2010 Stewart Smith

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "config.h"
#include <drizzled/plugin/function.h>
#include <drizzled/item/func.h>
#include "drizzled/charset.h"
#include <drizzled/function/str/strfunc.h>
#include "libinnodb_version_func.h"

#if defined(HAVE_HAILDB_H)
# include <haildb.h>
#else
# include <embedded_innodb-1.0/innodb.h>
#endif /* HAVE_HAILDB_H */

using namespace std;
using namespace drizzled;

class LibinnodbVersionFunction : public Item_str_func
{
public:
  LibinnodbVersionFunction() : Item_str_func() {}
  String *val_str(String*);

  void fix_length_and_dec()
  {
    max_length= 32;
  }

  const char *func_name() const
  {
    return "libinnodb_version";
  }

  bool check_argument_count(int n)
  {
    return (n == 0);
  }
};


String *LibinnodbVersionFunction::val_str(String *str)
{
  assert(fixed == true);

  if (str->alloc(50))
  {
    null_value= true;
    return 0;
  }

  null_value= false;

  uint64_t version= ib_api_version();

  int len= snprintf((char *) str->ptr(), 50,
                    "Release %"PRIx64", Revision %"PRIx64", Age %"PRIx64,
                    version >> 32,
                    (version >> 16) & 0xffff,
                    version & 0xffff);

  str->length(len);

  return str;
}


plugin::Create_function<LibinnodbVersionFunction> *libinnodb_version_func= NULL;

int libinnodb_version_func_initialize(module::Context &context)
{
  libinnodb_version_func= new plugin::Create_function<LibinnodbVersionFunction>("libinnodb_version");
  context.add(libinnodb_version_func);
  return 0;
}
