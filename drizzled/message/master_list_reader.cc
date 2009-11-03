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
#include <drizzled/message/master_list.pb.h>
using namespace std;

/*
  Example reader application for master.info data.
*/

static void printRecord(const drizzle::MasterList *list)
{
  int x;

  for (x= 0; x < list->record_size(); x++)
  {
    const drizzle::MasterList_Record record= list->record(x);

    cout << "HOSTNAME " << record.hostname() << endl;
    if (record.has_username())
      cout << "USERNAME " << record.username() << endl;
    if (record.has_password())
      cout << "PASSWORD " << record.password() << endl;
    if (record.has_port())
      cout << "PORT " << record.port() << endl;
    if (record.has_connect_retry())
      cout << "CONNECT RETRY " << record.connect_retry() << endl;
    if (record.has_log_name())
      cout << "LOG NAME " << record.log_name() << endl;
    if (record.has_log_position())
      cout << "LOG POSITION " << record.log_position() << endl;
    cout << endl;
  }
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2)
  {
    cerr << "Usage:  " << argv[0] << " master.info " << endl;
    return -1;
  }

  drizzle::MasterList list;

  {
    // Read the existing master.info file
    fstream input(argv[1], ios::in | ios::binary);
    if (!list.ParseFromIstream(&input))
    {
      cerr << "Failed to parse master.info." << endl;
      return -1;
    }
  }

  printRecord(&list);

  return 0;
}
