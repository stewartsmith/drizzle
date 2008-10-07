#include <iostream>
#include <fstream>
#include <string>
#include "master_list.pb.h"
using namespace std;

/* 
  Example reader application for master.info data.
*/

void printRecord(const drizzle::MasterList *list) 
{
  uint32_t x;

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
    // Read the existing address book.
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
