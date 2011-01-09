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

#include <iostream>
#include <fstream>
#include <string>
#include <drizzled/message/catalog.pb.h>

using namespace std;
using namespace drizzled;


/*
  Written from Google proto example
*/

static void printCatalog(const message::Catalog *catalog)
{
  cout << "CREATE CATALOG `" << catalog->name() << "` ";

  for (int option_nr=0; option_nr < catalog->engine().options_size(); option_nr++)
  {
    cout << " " << catalog->engine().options(option_nr).name() << " = "
         << "'" << catalog->engine().options(option_nr).state() << "'";
  }

  cout << ";" << endl;
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " CATALOG" << endl;
    return -1;
  }

  message::Catalog catalog;

  {
    // Read the existing address book.
    fstream input(argv[1], ios::in | ios::binary);
    if (!catalog.ParseFromIstream(&input))
    {
      cerr << "Failed to parse catalog." << endl;
      return -1;
    }
  }

  printCatalog(&catalog);

  return 0;
}
