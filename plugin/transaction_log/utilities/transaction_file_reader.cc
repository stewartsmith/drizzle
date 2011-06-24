/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 David Shrewsbury <shrewsbury.dave@gmail.com>
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

/**
 * @file
 * Implementation of the TransactionFileReader class.
 */

#include <config.h>
#include "transaction_file_reader.h"
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <boost/lexical_cast.hpp>
#include <google/protobuf/io/coded_stream.h>
#include <drizzled/definitions.h>
#include <drizzled/algorithm/crc32.h>
#include <drizzled/replication_services.h>
#include <drizzled/gettext.h>
#include <drizzled/util/convert.h>

using namespace std;
using namespace drizzled;
using namespace google;


TransactionFileReader::TransactionFileReader()
{
  raw_input= NULL;
  buffer= NULL;
  temp_buffer= NULL;
  previous_length= 0;
  file= -1;
}

TransactionFileReader::~TransactionFileReader()
{
  delete raw_input;
  close(file);
  free(buffer);
}

bool TransactionFileReader::openFile(const string &filename, int start_pos)
{
  file= open(filename.c_str(), O_RDONLY);
  if (file == -1)
  {
    error= _("Cannot open file: ") + filename;
    return false;
  }

  raw_input= new protobuf::io::FileInputStream(file);

  if (start_pos > 0)
  {
    if (not raw_input->Skip(start_pos))
    {
      error= _("Could not skip to position ")
               + boost::lexical_cast<string>(start_pos);
      return false;
    }
  }

  return true;
}

bool TransactionFileReader::getNextTransaction(message::Transaction &transaction,
                                              uint32_t *checksum)
{
  uint32_t message_type= 0;
  uint32_t length= 0;
  bool result= true;

  /*
   * Odd thing to note about using CodedInputStream: This class wasn't
   * intended to read large amounts of GPB messages. It has an upper
   * limit on the number of bytes it will read (see Protobuf docs for
   * this class for more info). A warning will be produced as you
   * get close to this limit. Since this is a pretty lightweight class,
   * we should be able to simply create a new one for each message we
   * want to read.
   */
  protobuf::io::CodedInputStream coded_input(raw_input);

  /* Read in the type and length of the command */
  if (not coded_input.ReadLittleEndian32(&message_type) ||
      not coded_input.ReadLittleEndian32(&length))
  {
    error= "EOF";
    return false;
  }

  if (message_type != ReplicationServices::TRANSACTION)
  {
    error= _("Found a non-transaction message in log. Currently, not supported.\n");
    return false;
  }

  if (length > INT_MAX)
  {
    error= _("Attempted to read record bigger than INT_MAX\n");
    return false;
  }

  if (buffer == NULL)
  {
    /*
     * First time around...just malloc the length.  This block gets rid
     * of a GCC warning about uninitialized temp_buffer.
     */
    temp_buffer= (char *) malloc(static_cast<size_t>(length));
  }
  /* No need to allocate if we have a buffer big enough... */
  else if (length > previous_length)
  {
    temp_buffer= (char *) realloc(buffer, static_cast<size_t>(length));
  }

  if (temp_buffer == NULL)
  {
    error= _("Memory allocation failure trying to allocate ")
            + boost::lexical_cast<string>(length)
            + _(" bytes\n");
    return false;
  }
  else
    buffer= temp_buffer;

  /* Read the Command */
  result= coded_input.ReadRaw(buffer, (int) length);

  if (result == false)
  {
    char errmsg[STRERROR_MAX];
    strerror_r(errno, errmsg, sizeof(errmsg));
    error= _("Could not read transaction message.\n");
    error += _("GPB ERROR: ") + string(errmsg) + "\n";
    string hexdump;
    hexdump.reserve(length * 4);
    bytesToHexdumpFormat(hexdump,
                         reinterpret_cast<const unsigned char *>(buffer),
                         length);
    error += _("HEXDUMP:\n\n") + hexdump;
    return false;
  }

  result= transaction.ParseFromArray(buffer, static_cast<int32_t>(length));

  if (result == false)
  {
    error= _("Unable to parse command. Got error: ")
             + transaction.InitializationErrorString();
    if (buffer != NULL)
    {
      string hexdump;
      hexdump.reserve(length * 4);
      bytesToHexdumpFormat(hexdump,
                           reinterpret_cast<const unsigned char *>(buffer),
                           length);
      error += _("\nHEXDUMP:\n\n") + hexdump + "\n";
    }
    return false;
  }

  /* Read 4 byte checksum */
  coded_input.ReadLittleEndian32(checksum);

  previous_length= length;

  return true;
}

uint32_t TransactionFileReader::checksumLastReadTransaction()
{
  return drizzled::algorithm::crc32(buffer,
                                    static_cast<size_t>(previous_length));
}
