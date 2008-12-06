#include <iostream>
#include <fstream>
#include <string>
#include "master_list.pb.h"
using namespace std;

/*
  Example script for reader a Drizzle master replication list.
*/

void fill_master_record(drizzle::MasterList_Record *record, const char *hostname)
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
