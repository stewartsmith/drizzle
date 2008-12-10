#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <uuid/uuid.h>

#include "replication_event.pb.h"
using namespace std;

static uint64_t query_id= 0;
char transaction_id[37];

/*
  Example script for reader a Drizzle master replication list.
*/

void write_ddl(drizzle::Event *record, const char *sql)
{
  uuid_t uu;

  uuid_generate_time(uu);
  uuid_unparse(uu, transaction_id);

  using namespace drizzle;
  record->set_type(Event::DDL);
  record->set_autocommit(true);
  record->set_server_id("localhost");
  record->set_query_id(query_id++);
  record->set_transaction_id(transaction_id);
  record->set_schema("test");
  record->set_sql(sql);
}

void write_insert(drizzle::Event *record, const char *trx)
{
  using namespace drizzle;
  Event::Value *value;

  record->set_type(Event::INSERT);
  record->set_autocommit(true);
  record->set_server_id("localhost");
  record->set_query_id(query_id++);
  record->set_transaction_id(trx);
  record->set_schema("test");
  record->set_table("t1");
  record->set_sql("INSERT INTO t1 (a) VALUES (1) (2)");

  /* Add Field Names */
  record->add_field_names("a");

  /* Add values (first row) */
  value= record->add_values();
  value->add_value("1");

  /* Add values (second row) */
  value= record->add_values();
  value->add_value("2");
}

void write_delete(drizzle::Event *record, const char *trx)
{
  using namespace drizzle;
  uuid_t uu;
  Event::Value *value;

  record->set_type(Event::DELETE);
  record->set_autocommit(true);
  record->set_server_id("localhost");
  record->set_query_id(query_id++);
  record->set_transaction_id(trx);
  record->set_schema("test");
  record->set_table("t1");
  record->set_sql("DELETE FROM t1 WHERE a IN (1, 2)");

  /* Add Field Names */
  record->set_primary_key("a");

  /* Add values for IN() */
  value= record->add_values();
  value->add_value("1");
  value->add_value("2");
}

void write_update(drizzle::Event *record, const char *trx)
{
  using namespace drizzle;
  Event::Value *value;

  record->set_type(Event::UPDATE);
  record->set_autocommit(true);
  record->set_server_id("localhost");
  record->set_query_id(query_id++);
  record->set_transaction_id(trx);
  record->set_schema("test");
  record->set_table("t1");
  record->set_sql("UPDATE t1 SET a=5 WHERE a = 1 ");
  record->set_primary_key("a");

  /* Add Field Names */
  record->add_field_names("a");

  /* Add values (first row) */
  value= record->add_values();
  value->add_value("1"); // The first value is always the primary key comparison value
  value->add_value("5");

  /* Add values (second row) */
  value= record->add_values();
  value->add_value("2");
  value->add_value("6");
}

void write_to_disk(int file, drizzle::EventList *list)
{
  std::string buffer;
  uint64_t length;
  size_t written;

  list->SerializePartialToString(&buffer);

  length= buffer.length();

  cout << "Writing record of " << length << "." << endl;

  if ((written= write(file, &length, sizeof(uint64_t))) != sizeof(uint64_t))
  {
    cerr << "Only wrote " << written << " out of " << length << "." << endl;
    exit(1);
  }

  if ((written= write(file, buffer.c_str(), length)) != length)
  {
    cerr << "Only wrote " << written << " out of " << length << "." << endl;
    exit(1);
  }
}


int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  int file;

  if (argc != 2) 
  {
    cerr << "Usage:  " << argv[0] << " REPLICATION_EVENT_LOG " << endl;
    return -1;
  }

  if ((file= open(argv[1], O_APPEND|O_CREAT|O_SYNC|O_WRONLY, S_IRWXU)) == -1)
  {
    cerr << "Can not open file: " << argv[0] << endl;
   exit(0);
  }

  drizzle::EventList list;

  /* Write first set of records */
  write_ddl(list.add_event(), "CREATE TABLE A (a int) ENGINE=innodb");
  write_insert(list.add_event(), transaction_id);

  write_to_disk(file, &list);

  /* Write Second set of records */
  write_ddl(list.add_event(), "CREATE TABLE A (a int) ENGINE=innodb");
  write_delete(list.add_event(), transaction_id);
  write_update(list.add_event(), transaction_id);

  write_to_disk(file, &list);

  close(file);

  return 0;
}
