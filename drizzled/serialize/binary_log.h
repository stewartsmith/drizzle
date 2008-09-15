// -*- Mode: C++ -*-

#ifndef BINARY_LOG_H_INCLUDED
#define BINARY_LOG_H_INCLUDED

#include "binary_log.pb.h"
#include "binlog_encoding.h"

#include <google/protobuf/io/zero_copy_stream.h>

#include <iosfwd>
#include <stdexcept>

namespace BinaryLog {
  using namespace google::protobuf;
  using namespace google::protobuf::io;

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

    Event(EventType type, Message *message)
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

    bool write(CodedOutputStream* out) const;
    void print(std::ostream& out) const;
    bool read(CodedInputStream* in);

  private:
    EventType m_type;
    Message *m_message;
  };
}

#endif /* BINARY_LOG_H_INCLUDED */
