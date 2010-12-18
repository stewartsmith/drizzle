/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <drizzled/message/schema.pb.h>

using namespace std;
using namespace drizzled;


/*
  Written from Google proto example
*/

static void printSchema(const message::Schema *schema)
{
  cout << "CREATE SCHEMA `" << schema->name() << "` ";
  if (schema->has_collation())
    cout << "COLLATE `" << schema->collation() << "` ";

  for (int option_nr=0; option_nr < schema->engine().options_size(); option_nr++)
  {
    cout << " " << schema->engine().options(option_nr).name() << " = "
         << "'" << schema->engine().options(option_nr).state() << "'";
  }

  cout << ";" << endl;
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " SCHEMA" << endl;
    return -1;
  }

  message::Schema schema;

  {
    // Read the existing address book.
    fstream input(argv[1], ios::in | ios::binary);
    if (!schema.ParseFromIstream(&input))
    {
      cerr << "Failed to parse schema." << endl;
      return -1;
    }
  }

  printSchema(&schema);

  return 0;
}
