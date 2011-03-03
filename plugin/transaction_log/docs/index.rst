Transaction Log
===============

Description
-----------

The Drizzle Transaction Log contains a sequence of
`Google Protocol Buffer <http://code.google.com/p/protobuf/>`_
messages that describe the transactions which have been run.


Configuration
-------------

Several server variables control the transaction log.

.. program:: drizzled

.. option:: --transaction-log.enable

   Enable transaction log.

.. option:: --transaction-log.enable-checksum

   Enable CRC32 Checksumming of each written transaction log entry

.. option:: --transaction-log.file=arg

  Path to the file to use for transaction log. The default will be
  :file:`transaction.log`.

.. option:: --transaction-log.use-replicator

   Name of the replicator plugin to use (default='default_replicator')

.. option:: --transaction-log.flush-frequency=arg

   * 0 : rely on operating system to sync log file (default)
   * 1 : sync file at each transaction write
   * 2 : sync log file once per second

.. option:: --transaction-log.num-write-buffers=arg

   Number of slots for in-memory write buffers (default=8).


Transaction Log Messages
------------------------

Replication events are recorded using messages in the `Google Protocol Buffer
<http://code.google.com/p/protobuf/>`_ (GPB) format. See the :doc:`Replication
documentation </replication>` for more information.


Transaction Log Format
----------------------

Currently, the transaction log file uses a simple, single-file, append-only
format. The format of each entry in the transaction log file is::

      4-bytes        4-bytes
  +--------------+--------------+----------------------+
  |  Entry Type  |    Length    |  Serialized Message  |
  +--------------+--------------+----------------------+

The fields in the entry are:

* **Entry Type** - Type of message this entry contains. Currently,
  this is always a Transaction GPB message.
* **Length** - The length, in bytes, of the serialized message.
* **Serialized Message** - The actual message data.
