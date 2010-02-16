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

#include "config.h"
#include <drizzled/function/str/strfunc.h>
#include <drizzled/function/str/load_file.h>
#include <drizzled/error.h>
#include <drizzled/data_home.h>
#include <drizzled/session.h>
#include "drizzled/internal/my_sys.h"

#include <fcntl.h>
#include <sys/stat.h>

namespace drizzled
{

String *Item_load_file::val_str(String *str)
{
  assert(fixed == 1);
  String *file_name;
  int file;
  struct stat stat_info;
  char path[FN_REFLEN];

  if (!(file_name= args[0]->val_str(str)))
    goto err;

  (void) internal::fn_format(path, file_name->c_ptr(), data_home_real, "",
		   MY_RELATIVE_PATH | MY_UNPACK_FILENAME);

  /* Read only allowed from within dir specified by secure_file_priv */
  if (opt_secure_file_priv &&
      strncmp(opt_secure_file_priv, path, strlen(opt_secure_file_priv)))
    goto err;

  if (stat(path, &stat_info))
    goto err;

  if (!(stat_info.st_mode & S_IROTH))
  {
    /* my_error(ER_TEXTFILE_NOT_READABLE, MYF(0), file_name->c_ptr()); */
    goto err;
  }
  if (stat_info.st_size > (long) current_session->variables.max_allowed_packet)
  {
    push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                        ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
                        func_name(), current_session->variables.max_allowed_packet);
    goto err;
  }
  if (tmp_value.alloc((size_t)stat_info.st_size))
    goto err;
  if ((file = internal::my_open(file_name->c_ptr(), O_RDONLY, MYF(0))) < 0)
    goto err;
  if (internal::my_read(file, (unsigned char*) tmp_value.ptr(), (size_t)stat_info.st_size, MYF(MY_NABP)))
  {
    internal::my_close(file, MYF(0));
    goto err;
  }
  tmp_value.length((size_t)stat_info.st_size);
  internal::my_close(file, MYF(0));
  null_value = 0;
  return(&tmp_value);

err:
  null_value = 1;
  return(0);
}


} /* namespace drizzled */
