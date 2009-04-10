#include <drizzled/global.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <drizzled/message/replication_event.pb.h>

using namespace std;
using namespace drizzled::message;

/*
  Example reader application for master.info data.
*/

void printRecord(const ::drizzled::message::EventList *list)
{
  int32_t e_size;

  for (e_size= 0; e_size < list->event_size(); e_size++)
  {
    const Event event= list->event(e_size);

    if (e_size != 0)
      cout << endl << "##########################################################################################" << endl << endl;

    switch (event.type())
    {
    case Event::DDL:
      cout << "DDL ";
      break;
    case Event::INSERT:
      {
        int32_t x;

        cout << "INSERT INTO " << event.table() << " (";

        for (x= 0; x < event.field_names_size() ; x++)
        {
          if (x != 0)
            cout << ", ";

          cout << event.field_names(x);
        }

        cout << ") VALUES " << endl;

        for (x= 0; x < event.values_size(); x++)
        {
          int32_t y;
          Event_Value values= event.values(x);

          if (x != 0)
            cout << ", ";

          cout << "(";
          for (y= 0; y < values.val_size() ; y++)
          {
            if (y != 0)
              cout << ", ";

            cout << "\"" << values.val(y) << "\"";
          }
          cout << ")";
        }

        cout << ";" << endl;
        break;
      }
    case Event::DELETE:
      {
        int32_t x;
        Event_Value values= event.values(0);

        cout << "DELETE FROM " << event.table() << " WHERE " << event.primary_key() << " IN (";

        for (x= 0; x < values.val_size() ; x++)
        {
          if (x != 0)
            cout << ", ";

          cout << "\"" << values.val(x) << "\"";
        }

        cout << ")" << endl;
        break;
      }
    case Event::UPDATE:
      {
        int32_t count;

        for (count= 0; count < event.values_size() ; count++)
        {
          int32_t x;
          Event_Value values= event.values(count);

          cout << "UPDATE "  << event.table() << " SET ";

          for (x= 1; x < values.val_size() ; x++)
          {
            if (x != 1)
              cout << ", ";

            cout << event.field_names(x - 1) << " = \"" << values.val(x) << "\"";
          }

          cout << " WHERE " << event.primary_key() << " = " << values.val(0) << endl;
        }

        break;
      }
    case Event::COMMIT:
      cout << "COMMIT" << endl;
      break;
    }
    cout << endl;

    if (event.has_sql())
      cout << "Original SQL: " << event.sql() << endl;

    cout << "AUTOCOMMIT: " << event.autocommit() << endl;
    cout << "Server id: " << event.server_id() << endl;
    cout << "Query id: " << event.query_id() << endl;
    cout << "Transaction id: " << event.transaction_id() << endl;
    cout << "Schema: " << event.schema() << endl;
    if (event.type() != Event::DDL)
      cout << "Table Name: " << event.table() << endl;
  }
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  int file;

  if (argc != 2)
  {
    cerr << "Usage:  " << argv[0] << " replication event log " << endl;
    return -1;
  }

  EventList list;

  if ((file= open(argv[1], O_RDONLY)) == -1)
  {
    cerr << "Can not open file: " << argv[0] << endl;
  }

  while (1)
  {
    uint64_t length;
    char *buffer= NULL;
    char *temp_buffer;

    /* Read the size */
    if (read(file, &length, sizeof(uint64_t)) != sizeof(uint64_t))
      break;

    if (length > SIZE_MAX)
    {
      cerr << "Attempted to read record bigger than SIZE_MAX" << endl;
      exit(1);
    }
    temp_buffer= (char *)realloc(buffer, (size_t)length);
    if (temp_buffer == NULL)
    {
      cerr << "Memory allocation failure trying to " << length << "."  << endl;
      exit(1);
    }
    buffer= temp_buffer;

    /* Read the record */
    if (read(file, buffer, (size_t)length) != (ssize_t)length)
    {
      cerr << "Could not read entire record." << endl;
      exit(1);
    }
    list.ParseFromArray(buffer, (int)length);

    /* Print the record */
    printRecord(&list);
  }


  return 0;
}
