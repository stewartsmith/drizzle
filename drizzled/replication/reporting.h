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

#ifndef DRIZZLED_RPL_REPORTING_H
#define DRIZZLED_RPL_REPORTING_H

/**
   Maximum size of an error message from a slave thread.
 */
#define MAX_SLAVE_ERRMSG      1024

/**
   Mix-in to handle the message logging and reporting for relay log
   info and master log info structures.

   By inheriting from this class, the class is imbued with
   capabilities to do slave reporting.
 */
class Slave_reporting_capability
{
public:
  /**
     Constructor.

     @param thread_name Printable name of the slave thread that is reporting.
   */
  Slave_reporting_capability(char const *thread_name)
    : m_thread_name(thread_name)
  {
  }

  /**
     Writes a message and, if it's an error message, to Last_Error
     (which will be displayed by SHOW SLAVE STATUS).

     @param level       The severity level
     @param err_code    The error code
     @param msg         The message (usually related to the error
                        code, but can contain more information), in
                        printf() format.
  */
  void report(loglevel level, int err_code, const char *msg, ...) const
    __attribute__((format(printf, 4, 5)));

  /**
     Clear errors. They will not show up under <code>SHOW SLAVE
     STATUS</code>.
   */
  void clear_error() {
    m_last_error.clear();
  }

  /**
     Error information structure.
   */
  class Error {
    friend class Slave_reporting_capability;
  public:
    Error()
    {
      clear();
    }

    void clear()
    {
      number= 0;
      message[0]= '\0';
    }

    /** Error code */
    uint32_t number;
    /** Error message */
    char message[MAX_SLAVE_ERRMSG];
  };

  Error const& last_error() const { return m_last_error; }

private:
  /**
     Last error produced by the I/O or SQL thread respectively.
   */
  mutable Error m_last_error;

  char const *const m_thread_name;
};

#endif /* DRIZZLED_RPL_REPORTING_H */
