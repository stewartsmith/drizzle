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
  Example script for reader a Drizzle master replication list.
*/

static void fill_master_record(drizzle::MasterList_Record *record, const char *hostname)
{
  using namespace drizzle;
  record->set_hostname(hostname);
  record->set_username("root");
  record->set_password("mine");
  record->set_port(3306);
  record->set_connect_retry(5);
  record->set_log_name("/tmp/foo");
  record->set_log_position(0);
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " MASTER_LIST" << endl;
    return -1;
  }

  drizzle::MasterList list;

  fill_master_record(list.add_record(), "master.example.com");
  fill_master_record(list.add_record(), "foo.example.com");

  fstream output(argv[1], ios::out | ios::trunc | ios::binary);
  if (!list.SerializeToOstream(&output))
  {
    cerr << "Failed to write master_list." << endl;
    return -1;
  }

  return 0;
}
