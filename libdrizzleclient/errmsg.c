/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

/* Error messages for MySQL clients */

#include "config.h"
#include <drizzled/gettext.h>
#include "errmsg.h"

const char *client_errors[]=
{
  N_("Unknown Drizzle error"),
  N_("Can't create UNIX socket (%d)"),
  N_("Can't connect to local Drizzle server through socket '%-.100s' (%d)"),
  N_("Can't connect to Drizzle server on '%-.100s' (%d)"),
  N_("Can't create TCP/IP socket (%d)"),
  N_("Unknown Drizzle server host '%-.100s' (%d)"),
  N_("Drizzle server has gone away"),
  N_("Protocol mismatch; server version = %d, client version = %d"),
  N_("Drizzle client ran out of memory"),
  N_("Wrong host info"),
  N_("Localhost via UNIX socket"),
  N_("%-.100s via TCP/IP"),
  N_("Error in server handshake"),
  N_("Lost connection to Drizzle server during query"),
  N_("Commands out of sync; you can't run this command now"),
  N_("Named pipe: %-.32s"),
  N_("Can't wait for named pipe to host: %-.64s  pipe: %-.32s (%lu)"),
  N_("Can't open named pipe to host: %-.64s  pipe: %-.32s (%lu)"),
  N_("Can't set state of named pipe to host: %-.64s  pipe: %-.32s (%lu)"),
  N_("Can't initialize character set %-.32s (path: %-.100s)"),
  N_("Got packet bigger than 'max_allowed_packet' bytes"),
  N_("Embedded server"),
  N_("Error on SHOW SLAVE STATUS:"),
  N_("Error on SHOW SLAVE HOSTS:"),
  N_("Error connecting to slave:"),
  N_("Error connecting to master:"),
  N_("SSL connection error"),
  N_("Malformed packet"),
  N_("(unused error message)"),
  N_("Invalid use of null pointer"),
  N_("Statement not prepared"),
  N_("No data supplied for parameters in prepared statement"),
  N_("Data truncated"),
  N_("No parameters exist in the statement"),
  N_("Invalid parameter number"),
  N_("Can't send long data for non-string/non-binary data types "
     "(parameter: %d)"),
  N_("Using unsupported buffer type: %d  (parameter: %d)"),
  N_("Shared memory: %-.100s"),
  N_("(unused error message)"),
  N_("(unused error message)"),
  N_("(unused error message)"),
  N_("(unused error message)"),
  N_("(unused error message)"),
  N_("(unused error message)"),
  N_("(unused error message)"),
  N_("(unused error message)"),
  N_("(unused error message)"),
  N_("Wrong or unknown protocol"),
  N_("Invalid connection handle"),
  N_("Connection using old (pre-4.1.1) authentication protocol refused "
     "(client option 'secure_auth' enabled)"),
  N_("Row retrieval was canceled by drizzle_stmt_close() call"),
  N_("Attempt to read column without prior row fetch"),
  N_("Prepared statement contains no metadata"),
  N_("Attempt to read a row while there is no result set associated with "
     "the statement"),
  N_("This feature is not implemented yet"),
  N_("Lost connection to Drizzle server while waiting for initial "
     "communication packet, system error: %d"),
  N_("Lost connection to Drizzle server while reading initial communication "
     "packet, system error: %d"),
  N_("Lost connection to Drizzle server while sending authentication "
     "information, system error: %d"),
  N_("Lost connection to Drizzle server while reading authorization "
     "information, system error: %d"),
  N_("Lost connection to Drizzle server while setting initial database, "
     "system error: %d"),
  N_("Statement closed indirectly because of a preceding %s() call"),
/* CR_NET_UNCOMPRESS_ERROR 08S01  */
  N_("Couldn't uncompress communication packet"),
/* CR_NET_READ_ERROR 08S01  */
  N_("Got an error reading communication packets"),
/* CR_NET_READ_INTERRUPTED 08S01  */
  N_("Got timeout reading communication packets"),
/* CR_NET_ERROR_ON_WRITE 08S01  */
  N_("Got an error writing communication packets"),
/* CR_NET_WRITE_INTERRUPTED 08S01  */
  N_("Got timeout writing communication packets"),
  ""
};


const char *
drizzleclient_get_client_error(unsigned int err_index)
{
  return _(client_errors[err_index]);
}
