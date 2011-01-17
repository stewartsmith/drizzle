/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <iostream>
#include <fstream>
#include <string>
#include <drizzled/message/catalog.pb.h>

using namespace std;
using namespace drizzled;

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  string file_name;
  message::Catalog catalog;

  if (argc < 2) 
  {
    cerr << "Usage:  " << argv[0] << " CATALOG" << endl;
    return -1;
  }

  if (argc == 3)
    file_name= argv[2];
  else
    file_name= argv[1];

  catalog.set_name(argv[1]);
  catalog.mutable_engine()->set_name("filesystem");
  catalog.set_creation_timestamp(time(NULL));
  catalog.set_update_timestamp(time(NULL));
  catalog.set_uuid("catalog_writer");
  catalog.set_version(1);

  fstream output(file_name.c_str(), ios::out | ios::trunc | ios::binary);

  if (not catalog.SerializeToOstream(&output))
  {
    cerr << "Failed to write catalog." << endl;
    return -1;
  }

  return 0;
}
