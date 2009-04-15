#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>

#include <sys/time.h>

#include <drizzled/message/transaction.pb.h>

/** 
 * @file Example script for writing transactions to a log file.
 */

using namespace std;
using namespace drizzled::message;

static uint32_t server_id= 1;
static uint64_t transaction_id= 0;

uint64_t getNanoTimestamp()
{
#ifdef HAVE_CLOCK_GETTIME
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (uint64_t) tp.tv_sec * 10000000
       + (uint64_t) tp.tv_nsec;
#else
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (uint64_t) tv.tv_sec * 10000000
       + (uint64_t) tv.tv_usec * 1000;
#endif
}

void writeCommit(drizzled::message::Command &record)
{
  record.set_type(Command::COMMIT);
  record.set_timestamp(getNanoTimestamp());

  drizzled::message::TransactionContext *trx= record.mutable_transaction_context();
  trx->set_server_id(server_id);
  trx->set_transaction_id(transaction_id);
}

void writeRollback(drizzled::message::Command &record)
{
  record.set_type(Command::ROLLBACK);
  record.set_timestamp(getNanoTimestamp());

  drizzled::message::TransactionContext *trx= record.mutable_transaction_context();
  trx->set_server_id(server_id);
  trx->set_transaction_id(transaction_id);
}

void writeStartTransaction(drizzled::message::Command &record)
{
  record.set_type(Command::START_TRANSACTION);
  record.set_timestamp(getNanoTimestamp());

  drizzled::message::TransactionContext *trx= record.mutable_transaction_context();
  trx->set_server_id(server_id);
  trx->set_transaction_id(transaction_id);
}

void writeInsert(drizzled::message::Command &record)
{
  record.set_type(Command::INSERT);
  record.set_sql("INSERT INTO t1 (a) VALUES (1) (2)");
  record.set_timestamp(getNanoTimestamp());
  record.set_schema("test");
  record.set_table("t1");

  drizzled::message::TransactionContext *trx= record.mutable_transaction_context();
  trx->set_server_id(server_id);
  trx->set_transaction_id(transaction_id);

  drizzled::message::InsertRecord *irecord= record.mutable_insert_record();

  /* Add Fields and Values... */

  Table::Field *field= irecord->add_insert_field();
  field->set_name("a");
  field->set_type(drizzled::message::Table::Field::VARCHAR);

  irecord->add_insert_value("1");
  irecord->add_insert_value("2");
}

void writeDeleteWithPK(drizzled::message::Command &record)
{
  record.set_type(Command::DELETE);
  record.set_sql("DELETE FROM t1 WHERE a = 1");
  record.set_timestamp(getNanoTimestamp());
  record.set_schema("test");
  record.set_table("t1");

  drizzled::message::TransactionContext *trx= record.mutable_transaction_context();
  trx->set_server_id(server_id);
  trx->set_transaction_id(transaction_id);

  drizzled::message::DeleteRecord *drecord= record.mutable_delete_record();

  Table::Field *field= drecord->add_where_field();
  field->set_name("a");
  field->set_type(drizzled::message::Table::Field::VARCHAR);

  drecord->add_where_value("1");
}

void writeUpdateWithPK(drizzled::message::Command &record)
{
  record.set_type(Command::UPDATE);
  record.set_sql("UPDATE t1 SET a = 5 WHERE a = 1;");
  record.set_timestamp(getNanoTimestamp());
  record.set_schema("test");
  record.set_table("t1");

  drizzled::message::TransactionContext *trx= record.mutable_transaction_context();
  trx->set_server_id(server_id);
  trx->set_transaction_id(transaction_id);

  drizzled::message::UpdateRecord *urecord= record.mutable_update_record();

  Table::Field *field;
  
  field= urecord->add_update_field();
  field->set_name("a");
  field->set_type(drizzled::message::Table::Field::VARCHAR);

  urecord->add_after_value("5");

  field= urecord->add_where_field();
  field->set_name("a");
  field->set_type(drizzled::message::Table::Field::VARCHAR);

  urecord->add_where_value("1");
}

void writeTransaction(int file, drizzled::message::Transaction &transaction)
{
  std::string buffer;
  size_t length;
  size_t written;

  drizzled::message::TransactionContext *trx= transaction.mutable_transaction_context();
  trx->set_server_id(server_id);
  trx->set_transaction_id(transaction_id);

  transaction.SerializeToString(&buffer);

  length= buffer.length();

  cout << "Writing transaction of " << length << " length." << endl;

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
    cerr << "Usage:  " << argv[0] << " TRANSACTION_LOG" << endl;
    return -1;
  }

  if ((file= open(argv[1], O_APPEND|O_CREAT|O_SYNC|O_WRONLY, S_IRWXU)) == -1)
  {
    cerr << "Can not open file: " << argv[0] << endl;
   exit(0);
  }

  /* Write a series of statements which test each type of record class */
  transaction_id++;

  /* Simple INSERT statement */
  Transaction transaction;
  transaction.set_start_timestamp(getNanoTimestamp());
  writeStartTransaction(*transaction.add_command());
  writeInsert(*transaction.add_command());
  writeCommit(*transaction.add_command());
  transaction.set_end_timestamp(getNanoTimestamp());

  writeTransaction(file, transaction);

  transaction.Clear();

  /* Write a DELETE and an UPDATE in one transaction */
  transaction_id++;
  transaction.set_start_timestamp(getNanoTimestamp());
  writeStartTransaction(*transaction.add_command());
  writeDeleteWithPK(*transaction.add_command());
  writeUpdateWithPK(*transaction.add_command());
  writeCommit(*transaction.add_command());
  transaction.set_end_timestamp(getNanoTimestamp());

  writeTransaction(file, transaction);

  close(file);

  return 0;
}
