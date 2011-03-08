*******************
Drizzle Replication
*******************

Replication events are recorded using messages in the `Google Protocol Buffer
<http://code.google.com/p/protobuf/>`_ (GPB) format. GPB messages can contain
sub-messages. There is a single main "envelope" message, Transaction, that
is passed to plugins that subscribe to the replication stream.

Configuration Options
#####################

**transaction_message_threshold**

    Controls the size, in bytes, of the Transaction messages. When a Transaction
    message exceeds this size, a new Transaction message with the same
    transaction ID will be created to continue the replication events.
    See :ref:`bulk-operations` below.


**replicate_query**

    Controls whether the originating SQL query will be included within each
    Statement message contained in the enclosing Transaction message. The
    default global value is FALSE which will not include the query in the
    messages. It can be controlled per session, as well. For example:

    .. code-block:: mysql

       drizzle> set @@replicate_query = 1;

    The stored query should be used as a guide only, and never executed
    on a slave to perform replication as this will lead to incorrect results.

Message Definitions
###################

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
#######################

The main "envelope" message which represents an atomic transaction
which changed the state of a server is the Transaction message class.

The Transaction message contains two pieces:

#. A TransactionContext message containing information about the
   transaction as a whole, such as the ID of the executing server,
   the start and end timestamp of the transaction, segmenting
   metadata and a unique identifier for the transaction.
#. A vector of Statement messages representing the distinct SQL
   statements which modified the state of the server.  The Statement
   message is, itself, a generic envelope message containing a
   sub-message which describes the specific data modification which
   occurred on the server (such as, for instance, an INSERT statement).

The Statement Message
#####################

The generic "envelope" message containing information common to each
SQL statement executed against a server (such as a start and end timestamp
and the type of the SQL statement) as well as a Statement subclass message
describing the specific data modification event on the server.

Each Statement message contains a type member which indicates how readers
of the Statement should construct the inner Statement subclass representing
a data change.

Statements are recorded separately as sometimes individual statements
have to be rolled back.


.. _bulk-operations:

How Bulk Operations Work
########################

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

A single transaction in the database can possibly be represented with
multiple protobuf Transaction messages if the message grows too large.
This can happen if you have a bulk transaction, or a single statement
affecting a very large number of rows, or just a large transaction with
many statements/changes.

For the first two examples, it is likely that the Statement sub-message
itself will get segmented, causing another Transaction message to be
created to hold the rest of the Statement's row changes. In these cases,
it is enough to look at the segment information stored in the Statement
message (see example below).

For the last example, the Statement sub-messages may or may not be
segmented, but we could still need to split the individual Statements up into
multiple Transaction messages to keep the Transaction message size from
growing too large. In this case, the segment information in the Statement
submessages is not helpful if the Statement isn't segmented. We need this
information in the Transaction message itself.

Segmenting a Single SQL Statement
*********************************

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

Suppose the following table:

.. code-block:: mysql

  CREATE TABLE test.person
  (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY
  , first_name VARCHAR(50)
  , last_name VARCHAR(50)
  , is_active CHAR(1) NOT NULL DEFAULT 'Y'
  );

Also suppose that test.t1 contains 1 million records.

Next, suppose a client issues the SQL statement:

.. code-block:: mysql

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
#. Construct an UpdateData message.  Set the *segment_id* member to 1.
   Set the *end_segment* member to true.
#. For every record updated in a storage engine, the ReplicationServices
   component builds a new UpdateRecord message and appends this message
   to the aforementioned UpdateData message's record vector.
#. After a certain threshold of records is reached, the
   ReplicationServices component sets the current UpdateData message's
   *end_segment* member to false, and proceeds to send the Transaction
   message to replicators.
#. The ReplicationServices component then constructs a new Transaction
   message and constructs a transaction context with the same
   transaction ID and server information.
#. A new UpdateData message is created.  The message's *segment_id* is
   set to N+1 and as new records are updated, new UpdateRecord messages
   are appended to the UpdateData message's record vector.
#. While records are being updated, we repeat steps 5 through 7, with
   only the final UpdateData message having its *end_segment* member set
   to true.

Segmenting a Transaction
************************

The Transaction protobuf message also contains *segment_id* member and a
*end_segment* member. These values are also set appropriately when a
Statement sub-message is segmented, as described above.

These values are also set when a Transaction must be segmented along
individual Statement boundaries (i.e., the Statement message itself
is **not** segmented). In either case, it is enough to check the
*end_segment* and *segment_id* values of the Transaction message
to determine if this is a multi-message transaction.

Handling ROLLBACKs
##################

Both transactions and individual statements may be rolled back.

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
  ROLLBACK. This signifies to rollback the entire transaction.

A special Statement message type, ROLLBACK_STATEMENT, is used when
we have a segmented Statement message (see above) and we need to tell the
receiver to undo any changes made for this single statement, but not
for the entire transaction. If the receiver cannot handle rolling back
a single statement, then a message buffering strategy should be employed 
to guarantee that a statement was indeed applied successfully before
executing on the receiver.

.. _replication_streams:

Replication Streams
###################

The Drizzle kernel handles delivering replication messages to plugins by
maintaining a list of replication streams. A stream is represented as a
registered *replicator* and *applier* pair.

When a replication message is generated within the kernel, the replication
services module of the kernel will send this message to each registered
*replicator*. The *replicator* will then do something useful with it and
send it to each *applier* with which it is associated.

Replicators
***********

A registered *replicator* is a plugin that implements the TransactionReplicator
API. Each replicator will be plugged into the kernel to receive the Google
Protobuf messages that are generated as the database is changed. Ideally,
each registered replicator will transform or modify the messages it receives
to implement a specific behavior. For example, filtering by schema name.

Each registered replicator should have a unique name. The default replicator,
cleverly named **default_replicator**, does no transformation at all on the
replication messages.

Appliers
********

A registered *applier* is a plugin that implements the TransactionApplier
API. Appliers are responsible for applying the replication messages that it
will receive from a registered replicator. The word "apply" is used loosely
here. An applier may do anything with the replication messages that provides
useful behavior. For example, an applier may simply write the messages to a
file on disk, or it may send the messages over the network to some other
service to be processed.

At the point of registration with the Drizzle kernel, each applier specifies
the name of a registered replicator that it should be attached to in order to
make the replication stream pair.
