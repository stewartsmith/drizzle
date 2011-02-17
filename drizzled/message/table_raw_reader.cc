/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <drizzled/message/table.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

using namespace std;
using namespace drizzled;
using namespace google;


static void print_table(const message::Table &table)
{
  string proto_as_text("");

  protobuf::TextFormat::PrintToString(table, &proto_as_text);

  cout << proto_as_text;
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s SCHEMA\n", argv[0]);
    return -1;
  }

  message::Table table;

  {
    int fd= open(argv[1], O_RDONLY);

    if (fd == -1)
    {
      perror("Failed to open table definition file");
      return -1;
    }

    protobuf::io::ZeroCopyInputStream* input=
      new protobuf::io::FileInputStream(fd);

    if (!table.ParseFromZeroCopyStream(input))
    {
      fprintf(stderr, "Failed to parse table.");
      close(fd);
      return -1;
    }

    close(fd);
  }

  print_table(table);

  return 0;
}
