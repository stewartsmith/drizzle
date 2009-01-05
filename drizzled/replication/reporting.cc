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

#include <drizzled/server_includes.h>
#include <drizzled/replication/reporting.h>
#include <drizzled/gettext.h>
#include <drizzled/errmsg_print.h>

void
Slave_reporting_capability::report(loglevel level, int err_code,
                                   const char *msg, ...) const
{
  char buff[MAX_SLAVE_ERRMSG];
  char *pbuff= buff;
  uint32_t pbuffsize= sizeof(buff);
  va_list args;

  /*
    If it is an error,
    it must be reported in Last_error and Last_errno in
    SHOW SLAVE STATUS.
  */

  if (level == ERROR_LEVEL)
  {
    pbuff= m_last_error.message;
    pbuffsize= sizeof(m_last_error.message);
    m_last_error.number= err_code;
  }

  va_start(args, msg);
  vsnprintf(pbuff, pbuffsize, msg, args);
  va_end(args);

  /* If the msg string ends with '.', do not add a ',' it would be ugly */
  /* todo: check type loglevel against errmsg priority */
  errmsg_printf((int) level,
		_("Slave %s: %s%s Error_code: %d"),
		m_thread_name, pbuff,
		(pbuff[0] && *(strchr(pbuff, '\0')-1) == '.') ? "" : ",",
		err_code);
}
