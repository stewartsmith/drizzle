/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef DRIZZLED_MESSAGE_BINARY_LOG_H
#define DRIZZLED_MESSAGE_BINARY_LOG_H

#include <drizzled/message/binary_log.pb.h>
#include "binlog_encoding.h"

#include <google/protobuf/io/zero_copy_stream.h>

#include <iosfwd>
#include <stdexcept>

namespace BinaryLog {

  /**
     Wrapper class to wrap a protobuf event in a type tag and a length.

     The type tag is not part of the actual message, but is handled
     separately since it is needed to decode the events.
  */
  class Event {
  public:
    enum EventType {
      UNDEF,
      START,
      CHAIN,
      COMMIT,
      ROLLBACK,
      QUERY,
      COUNT
    };

    Event(EventType type, google::protobuf::Message *message)
      : m_type(type), m_message(message)
    {
    }

    Event()
      : m_type(UNDEF), m_message(0)
    {
    }

    ~Event() {
      delete m_message;
    }

    bool write(google::protobuf::io::CodedOutputStream* out) const;
    void print(std::ostream& out) const;
    bool read(google::protobuf::io::CodedInputStream* in);

  private:
    EventType m_type;
    google::protobuf::Message *m_message;
  };
}

#endif /* DRIZZLED_MESSAGE_BINARY_LOG_H */
