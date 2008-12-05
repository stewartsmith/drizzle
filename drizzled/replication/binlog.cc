/* Copyright (C) 2005-2006 MySQL AB

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

#include <drizzled/server_includes.h>
#include <drizzled/replication/rli.h>
#include <drizzled/replication/binlog.h>
#include <mysys/base64.h>
#include <drizzled/error.h>
#include <drizzled/slave.h>
#include <drizzled/session.h>

/**
  Execute a BINLOG statement

  To execute the BINLOG command properly the server needs to know
  which format the BINLOG command's event is in.  Therefore, the first
  BINLOG statement seen must be a base64 encoding of the
  Format_description_log_event, as outputted by mysqlbinlog.  This
  Format_description_log_event is cached in
  rli->description_event_for_exec.
*/

void mysql_client_binlog_statement(Session* session)
{
  size_t coded_len= session->lex->comment.length + 1;
  size_t decoded_len= base64_needed_decoded_length(coded_len);
  assert(coded_len > 0);

  /*
    Allocation
  */

  /*
    If we do not have a Format_description_event, we create a dummy
    one here.  In this case, the first event we read must be a
    Format_description_event.
  */
  bool have_fd_event= true;
  if (!session->rli_fake)
  {
    session->rli_fake= new Relay_log_info;
#ifdef HAVE_purify
    session->rli_fake->is_fake= true;
#endif
    have_fd_event= false;
  }
  if (session->rli_fake && !session->rli_fake->relay_log.description_event_for_exec)
  {
    session->rli_fake->relay_log.description_event_for_exec=
      new Format_description_log_event(4);
    have_fd_event= false;
  }

  const char *error= 0;
  char *buf= (char *) malloc(decoded_len);
  Log_event *ev = 0;

  /*
    Out of memory check
  */
  if (!(session->rli_fake &&
        session->rli_fake->relay_log.description_event_for_exec &&
        buf))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), 1);  /* needed 1 bytes */
    goto end;
  }

  session->rli_fake->sql_session= session;
  session->rli_fake->no_storage= true;

  for (char const *strptr= session->lex->comment.str ;
       strptr < session->lex->comment.str + session->lex->comment.length ; )
  {
    char const *endptr= 0;
    int bytes_decoded= base64_decode(strptr, coded_len, buf, &endptr);

    if (bytes_decoded < 0)
    {
      my_error(ER_BASE64_DECODE_ERROR, MYF(0));
      goto end;
    }
    else if (bytes_decoded == 0)
      break; // If no bytes where read, the string contained only whitespace

    assert(bytes_decoded > 0);
    assert(endptr > strptr);
    coded_len-= endptr - strptr;
    strptr= endptr;

    /*
      Now we have one or more events stored in the buffer. The size of
      the buffer is computed based on how much base64-encoded data
      there were, so there should be ample space for the data (maybe
      even too much, since a statement can consist of a considerable
      number of events).

      TODO: Switch to use a stream-based base64 encoder/decoder in
      order to be able to read exactly what is necessary.
    */

    /*
      Now we start to read events of the buffer, until there are no
      more.
    */
    for (char *bufptr= buf ; bytes_decoded > 0 ; )
    {
      /*
        Checking that the first event in the buffer is not truncated.
      */
      uint32_t event_len= uint4korr(bufptr + EVENT_LEN_OFFSET);
      if (bytes_decoded < EVENT_LEN_OFFSET || (uint) bytes_decoded < event_len)
      {
        my_error(ER_SYNTAX_ERROR, MYF(0));
        goto end;
      }

      /*
        If we have not seen any Format_description_event, then we must
        see one; it is the only statement that can be read in base64
        without a prior Format_description_event.
      */
      if (!have_fd_event)
      {
        int type = bufptr[EVENT_TYPE_OFFSET];
        if (type == FORMAT_DESCRIPTION_EVENT || type == START_EVENT_V3)
          have_fd_event= true;
        else
        {
          my_error(ER_NO_FORMAT_DESCRIPTION_EVENT_BEFORE_BINLOG_STATEMENT,
                   MYF(0), Log_event::get_type_str((Log_event_type)type));
          goto end;
        }
      }

      ev= Log_event::read_log_event(bufptr, event_len, &error,
                                    session->rli_fake->relay_log.
                                      description_event_for_exec);

      if (!ev)
      {
        /*
          This could actually be an out-of-memory, but it is more likely
          causes by a bad statement
        */
        my_error(ER_SYNTAX_ERROR, MYF(0));
        goto end;
      }

      bytes_decoded -= event_len;
      bufptr += event_len;

      ev->session= session;
      /*
        We go directly to the application phase, since we don't need
        to check if the event shall be skipped or not.

        Neither do we have to update the log positions, since that is
        not used at all: the rli_fake instance is used only for error
        reporting.
      */
      if (apply_event_and_update_pos(ev, session, session->rli_fake, false))
      {
        /*
          TODO: Maybe a better error message since the BINLOG statement
          now contains several events.
        */
        my_error(ER_UNKNOWN_ERROR, MYF(0), "Error executing BINLOG statement");
        goto end;
      }

      /*
        Format_description_log_event should not be deleted because it
        will be used to read info about the relay log's format; it
        will be deleted when the SQL thread does not need it,
        i.e. when this thread terminates.
      */
      if (ev->get_type_code() != FORMAT_DESCRIPTION_EVENT)
        delete ev;
      ev= 0;
    }
  }

  my_ok(session);

end:
  session->rli_fake->clear_tables_to_lock();
  free(buf);
  return;
}
