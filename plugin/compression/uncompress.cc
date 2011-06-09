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

#include <drizzled/error.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/session.h>
#include <drizzled/system_variables.h>
#include <plugin/compression/uncompress.h>

#include <zlib.h>
#include <string>

using namespace std;
using namespace drizzled;

String *Item_func_uncompress::val_str(String *str)
{
  assert(fixed == 1);
  String *res= args[0]->val_str(str);
  ulong new_size;
  int err;
  drizzled::error_t code;

  if (!res)
    goto err;
  null_value= 0;
  if (res->is_empty())
    return res;

  /* If length is less than 4 bytes, data is corrupt */
  if (res->length() <= 4)
  {
    push_warning_printf(&getSession(), DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_ZLIB_Z_DATA_ERROR,
                        ER(ER_ZLIB_Z_DATA_ERROR));
    goto err;
  }

  /* Size of uncompressed data is stored as first 4 bytes of field */
  new_size= uint4korr(res->ptr()) & 0x3FFFFFFF;
  if (new_size > getSession().variables.max_allowed_packet)
  {
    push_warning_printf(&getSession(), DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_TOO_BIG_FOR_UNCOMPRESS,
                        ER(ER_TOO_BIG_FOR_UNCOMPRESS),
                        getSession().variables.max_allowed_packet);
    goto err;
  }

  buffer.realloc(new_size);

  if ((err= uncompress((Byte*)buffer.ptr(), &new_size,
                       ((const Bytef*)res->ptr())+4,res->length())) == Z_OK)
  {
    buffer.length((uint32_t) new_size);
    return &buffer;
  }

  code= ((err == Z_BUF_ERROR) ? ER_ZLIB_Z_BUF_ERROR :
         ((err == Z_MEM_ERROR) ? ER_ZLIB_Z_MEM_ERROR : ER_ZLIB_Z_DATA_ERROR));
  push_warning(&getSession(), DRIZZLE_ERROR::WARN_LEVEL_ERROR, code, ER(code));

err:
  null_value= 1;
  return 0;
}

