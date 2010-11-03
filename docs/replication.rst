Drizzle Replication
===================

Replication events are recorded using messages in the `Google Protocol Buffer
<http://code.google.com/p/protobuf/>`_ (GPB) format. GPB messages can contain
sub-messages. There is a single main "envelope" message, Transaction, that
is passed to plugins that subscribe to the replication stream.


Message Definitions
-------------------

The GPB messages are defined in .proto files in the drizzled/message
directory of the Drizzle source code. The primary definition file is
transaction.proto. Messages defined in this file are related in the
following ways::


  ------------------------------------------------------------------
  |                                                                |
  | Transaction message                                            |
  |                                                                |
  |   -----------------------------------------------------------  |
  |   |                                                         |  |
  |   | TransactionContext message                              |  |
  |   |                                                         |  |
  |   -----------------------------------------------------------  |
  |   -----------------------------------------------------------  |
  |   |                                                         |  |
  |   | Statement message 1                                     |  |
  |   |                                                         |  |
  |   -----------------------------------------------------------  |
  |   -----------------------------------------------------------  |
  |   |                                                         |  |
  |   | Statement message 2                                     |  |
  |   |                                                         |  |
  |   -----------------------------------------------------------  |
  |                             ...                                |
  |   -----------------------------------------------------------  |
  |   |                                                         |  |
  |   | Statement message N                                     |  |
  |   |                                                         |  |
  |   -----------------------------------------------------------  |
  ------------------------------------------------------------------

with each Statement message looking like so::

  ------------------------------------------------------------------
  |                                                                |
  | Statement message                                              |
  |                                                                |
  |   -----------------------------------------------------------  |
  |   |                                                         |  |
  |   | Common information                                      |  |
  |   |                                                         |  |
  |   |  - Type of Statement (INSERT, DELETE, etc)              |  |
  |   |  - Start Timestamp                                      |  |
  |   |  - End Timestamp                                        |  |
  |   |  - (OPTIONAL) Actual SQL query string                   |  |
  |   |                                                         |  |
  |   -----------------------------------------------------------  |
  |   -----------------------------------------------------------  |
  |   |                                                         |  |
  |   | Statement subclass message 1 (see below)                |  |
  |   |                                                         |  |
  |   -----------------------------------------------------------  |
  |                             ...                                |
  |   -----------------------------------------------------------  |
  |   |                                                         |  |
  |   | Statement subclass message N (see below)                |  |
  |   |                                                         |  |
  |   -----------------------------------------------------------  |
  ------------------------------------------------------------------

The Transaction Message
^^^^^^^^^^^^^^^^^^^^^^^

The main "envelope" message which represents an atomic transaction
which changed the state of a server is the Transaction message class.

The Transaction message contains two pieces:

#. A TransactionContext message containing information about the
   transaction as a whole, such as the ID of the executing server,
   the start and end timestamp of the transaction, and a globally-
   unique identifier for the transaction.
#. A vector of Statement messages representing the distinct SQL
   statements which modified the state of the server.  The Statement
   message is, itself, a generic envelope message containing a
   sub-message which describes the specific data modification which
   occurred on the server (such as, for instance, an INSERT statement.

The Statement Message
^^^^^^^^^^^^^^^^^^^^^

The generic "envelope" message containing information common to each
SQL statement executed against a server (such as a start and end timestamp
and the type of the SQL statement) as well as a Statement subclass message
describing the specific data modification event on the server.

Each Statement message contains a type member which indicates how readers
of the Statement should construct the inner Statement subclass representing
a data change.


How Bulk Operations Work
------------------------

Certain operations which change large volumes of data on a server
present a specific set of problems for a transaction coordinator or
replication service. If all operations must complete atomically on a
publishing server before replicas are delivered the complete
transactional unit:

#. The publishing server could consume a large amount of memory
   building an in-memory Transaction message containing all the
   operations contained  in the entire transaction.
#. A replica, or subscribing server, is wasting time waiting on the
   eventual completion (commit) of the large transaction on the
   publishing server. It could be applying pieces of the large
   transaction in the meantime...

In order to prevent the problems inherent in (1) and (2) above, Drizzle's
replication system uses a mechanism which provides bulk change
operations.

When a regular SQL statement modifies or inserts more rows than a
certain threshold, Drizzle's replication services component will begin
sending Transaction messages to replicas which contain a chunk
(or "segment") of the data which has been changed on the publisher.

When data is inserted, updated, or modified in the database, a
header containing information about modified tables and fields is
matched with one or more data segments which contain the actual
values changed in the statement.

It's easiest to understand this mechanism by following through a real-world
scenario.

Suppose the following table::

  CREATE TABLE test.person
  (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY
  , first_name VARCHAR(50)
  , last_name VARCHAR(50)
  , is_active CHAR(1) NOT NULL DEFAULT 'Y'
  );

Also suppose that test.t1 contains 1 million records.

Next, suppose a client issues the SQL statement::

  UPDATE test.person SET is_active = 'N';

It is clear that one million records could be updated by this statement
(we say, "could be" since Drizzle does not actually update a record if
the UPDATE would not change the existing record...).

In order to prevent the publishing server from having to construct an
enormous Transaction message, Drizzle's replication services component
will do the following:

#. Construct a Transaction message with a transaction context containing
   information about the originating server, the transaction ID, and
   timestamp information.
#. Construct an UpdateHeader message with information about the tables
   and fields involved in the UPDATE statement.  Push this UpdateHeader
   message onto the Transaction message's statement vector.
#. Construct an UpdateData message.  Set the segment_id member to 1.
   Set the end_segment member to true.
#. For every record updated in a storage engine, the ReplicationServices
   component builds a new UpdateRecord message and appends this message
   to the aforementioned UpdateData message's record vector.
#. After a certain threshold of records is reached, the
   ReplicationServices component sets the current UpdateData message's
   end_segment member to false, and proceeds to send the Transaction
   message to replicators.
#. The ReplicationServices component then constructs a new Transaction
   message and constructs a transaction context with the same
   transaction ID and server information.
#. A new UpdateData message is created.  The message's segment_id is
   set to N+1 and as new records are updated, new UpdateRecord messages
   are appended to the UpdateData message's record vector.
#. While records are being updated, we repeat steps 5 through 7, with
   only the final UpdateData message having its end_segment member set
   to true.

Handling ROLLBACKs
------------------

When a transaction is rolled back, one of two things happen depending
on whether the transaction is made up of either a single Transaction
message, or if it is made up of multiple Transaction messages (e.g, bulk
load).

* For a transaction encapsulated entirely within a single Transaction
  message, the entire message is simply discarded and not sent through
  the replication stream.
* For a transaction which is made up of multiple messages, and at least
  one message has already been sent through the replication stream, then
  the Transaction message will contain a Statement message with type =
  ROLLBACK.