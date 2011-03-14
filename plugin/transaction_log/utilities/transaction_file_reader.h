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
 * Declaration of a class used to read the GPB messages written
 * by transaction_writer.
 */

#pragma once

#include <string>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <drizzled/message/transaction.pb.h>

class TransactionFileReader
{
public:
  TransactionFileReader();
  ~TransactionFileReader();

  /**
   * Open the given transaction log file.
   *
   * @param filename Name of the file to open
   * @param start_pos Position within the file to begin reading
   *
   * @retval true Success
   * @retval false Failure
   */
  bool openFile(const std::string &filename, int start_pos = 0);

  /**
   * Read in next Transaction message and checksum.
   *
   * @note Error message will be "EOF" when end-of-file is reached.
   *
   * @param transaction Storage for the Transaction message
   * @param checksum Storage for the checksum value
   *
   * @retval true Success
   * @retval false Failure or EOF
   */
  bool getNextTransaction(drizzled::message::Transaction &transaction,
                          uint32_t *checksum);

  /**
   * Perform a checksum of the last read transaction message.
   */
  uint32_t checksumLastReadTransaction();

  /**
   * Get the error message from the last failed operation.
   */
  std::string getErrorString()
  {
    return error;
  }

private:
  int file;
  char *buffer;
  char *temp_buffer;
  uint32_t previous_length;
  std::string error;
  google::protobuf::io::ZeroCopyInputStream *raw_input;
};

