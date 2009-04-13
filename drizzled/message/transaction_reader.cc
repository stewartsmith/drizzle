#include <drizzled/global.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <drizzled/message/transaction.pb.h>

using namespace std;
using namespace drizzled::message;

/**
 * @file Example application for reading change records and transactions
 */

void printInsert(const drizzled::message::Command &container, const drizzled::message::InsertRecord &record)
{

  cout << "INSERT INTO `" << container.schema() << "`.`" << container.table() << "` (";

  int32_t num_fields= record.insert_field_size();

  int32_t x;
  for (x= 0; x < num_fields; x++)
  {
    if (x != 0)
      cout << ", ";

    const Table::Field f= record.insert_field(x);

    cout << "`" << f.name() << "`";
  }

  cout << ") VALUES ";

  /* 
   * There may be an INSERT VALUES (),() type statement.  We know the
   * number of records is equal to the field_values array size divided
   * by the number of fields.
   *
   * So, we do an inner and an outer loop.  Outer loop is on the number
   * of records and the inner loop on the number of fields.  In this way, 
   * we know that record.field_values(outer_loop * num_fields) + inner_loop))
   * always gives us our correct field value.
   */
  int32_t num_records= (record.insert_value_size() / num_fields);
  int32_t y;
  for (x= 0; x < num_records; x++)
  {
    if (x != 0)
      cout << ", ";

    cout << "(";
    for (y= 0; y < num_fields; y++)
    {
      if (y != 0)
        cout << ", ";

      cout << "\"" << record.insert_value((x * num_fields) + y) << "\"";
    }
    cout << ")";
  }

  cout << ";";
}

void printDeleteWithPK(const drizzled::message::Command &container, const drizzled::message::DeleteRecord &record)
{
  cout << "DELETE FROM `" << container.schema() << "`.`" << container.table() << "`";

  int32_t num_where_fields= record.where_field_size();
  /* 
   * Make sure we catch anywhere we're not aligning the fields with
   * the field_values arrays...
   */
  assert(num_where_fields == record.where_value_size());

  cout << " WHERE ";
  int32_t x;
  for (x= 0; x < num_where_fields; x++)
  {
    if (x != 0)
      cout << " AND "; /* Always AND condition with a multi-column PK */

    const Table::Field f= record.where_field(x);

    /* Always equality conditions */
    cout << "`" << f.name() << "` = \"" << record.where_value(x) << "\"";
  }
}

void printUpdateWithPK(const drizzled::message::Command &container, const drizzled::message::UpdateRecord &record)
{
  int32_t num_update_fields= record.update_field_size();
  int32_t x;

  cout << "UPDATE `" << container.schema() << "`.`" << container.table() << "` SET ";

  for (x= 0;x < num_update_fields; x++)
  {
    Table::Field f= record.update_field(x);
    
    if (x != 0)
      cout << ", ";

    cout << "`" << f.name() << "` = \"" << record.after_value(x) << "\"";
  }

  int32_t num_where_fields= record.where_field_size();
  /* 
   * Make sure we catch anywhere we're not aligning the fields with
   * the field_values arrays...
   */
  assert(num_where_fields == record.where_value_size());

  cout << " WHERE ";
  for (x= 0;x < num_where_fields; x++)
  {
    if (x != 0)
      cout << " AND "; /* Always AND condition with a multi-column PK */

    const Table::Field f= record.where_field(x);

    /* Always equality conditions */
    cout << "`" << f.name() << "` = \"" << record.where_value(x) << "\"";
  }
}

void printTransaction(const drizzled::message::Transaction &transaction)
{
  int32_t e_size;

  cout << "/* Start Time: " << transaction.start_timestamp() << " */ START TRANSACTION;"<< endl;

  for (e_size= 0; e_size < transaction.command_size(); e_size++)
  {
    const drizzled::message::Command command= transaction.command(e_size);

    drizzled::message::TransactionContext trx= command.transaction_context();

    cout << "/* SID: " << trx.server_id() << " XID: " << trx.transaction_id() << " */ ";

    switch (command.type())
    {
      case Command::START_TRANSACTION:
        cout << "START TRANSACTION;";
        break;
      case Command::COMMIT:
        cout << "COMMIT;";
        break;
      case Command::ROLLBACK:
        cout << "ROLLBACK;";
        break;
      case Command::INSERT:
      {
        printInsert(command, command.insert_record());
        break;
      }
      case Command::DELETE:
      {
        printDeleteWithPK(command, command.delete_record());
        break;
      }
      case Command::UPDATE:
      {
        printUpdateWithPK(command, command.update_record());
        break;
      }
      default:
      assert(0);
    }
    cout << endl;
  }
  cout << "/* Commit Time: " << transaction.end_timestamp() << " */ COMMIT;" << endl;
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

  Transaction transaction;

  if ((file= open(argv[1], O_RDONLY)) == -1)
  {
    cerr << "Can not open file: " << argv[1] << endl;
  }

  char *buffer= NULL;
  char *temp_buffer;

  while (1)
  {
    uint64_t length;

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
      cerr << "Memory allocation failure trying to allocate " << length << " bytes."  << endl;
      exit(1);
    }
    memset(temp_buffer, 0, length);
    buffer= temp_buffer;
    size_t read_bytes= 0;

    /* Read the transaction */
    if ((read_bytes= read(file, buffer, (uint64_t)length)) != (uint64_t)length)
    {
      cerr << "Could not read entire transaction. Read " << read_bytes << " bytes instead of " << length << " bytes." << endl;
      exit(1);
    }
    transaction.ParseFromArray(buffer, (int) length);

    /* Print the transaction */
    printTransaction(transaction);
  }
  return 0;
}
