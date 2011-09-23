Transaction Log
===============

The Drizzle Transaction Log contains a sequence of
`Google Protocol Buffer <http://code.google.com/p/protobuf/>`_
messages that describe the transactions which have been run.

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`transaction_log_configuration` and
:ref:`transaction_log_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=transaction_log

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _transaction_log_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --transaction-log.enable

   :Default: false
   :Variable: :ref:`transaction_log_enable <transaction_log_enable>`

   Enable transaction log.

.. option:: --transaction-log.enable-checksum 

   :Default: false
   :Variable: :ref:`transaction_log_enable_checksum <transaction_log_enable_checksum>`

   Enable CRC32 Checksumming of each written transaction log entry

.. option:: --transaction-log.file ARG

   :Default: :file:`transaction.log`
   :Variable: :ref:`transaction_log_file <transaction_log_file>`

   Path to the file to use for transaction log.

.. option:: --transaction-log.flush-frequency=arg

   :Default: ``0``
   :Variable: :ref:`transaction_log_flush_frequency <transaction_log_flush_frequency>`

   * 0 : rely on operating system to sync log file (default)
   * 1 : sync file at each transaction write
   * 2 : sync log file once per second

.. option:: --transaction-log.num-write-buffers ARG

   :Default: 8
   :Variable: :ref:`transaction_log_num_write_buffers <transaction_log_num_write_buffers>`

   Number of slots for in-memory write buffers.

.. option:: --transaction-log.truncate-debug 

   :Default: false
   :Variable: :ref:`transaction_log_truncate_debug <transaction_log_truncate_debug>`

   DEBUGGING - Truncate transaction log.

.. option:: --transaction-log.use-replicator ARG

   :Default: ``default_replicator``
   :Variable: :ref:`transaction_log_use_replicator <transaction_log_use_replicator>`

   Name of the replicator plugin to use.

.. _transaction_log_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _transaction_log_enable:

* ``transaction_log_enable``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--transaction-log.enable`

   If the transaction log is enabled or not.

.. _transaction_log_enable_checksum:

* ``transaction_log_enable_checksum``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--transaction-log.enable-checksum`

   Enable CRC32 Checksumming of each written transaction log entry

.. _transaction_log_file:

* ``transaction_log_file``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--transaction-log.file`

   Path to the file to use for transaction log

.. _transaction_log_flush_frequency:

* ``transaction_log_flush_frequency``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--transaction-log.flush-frequency`

   Flush frequency.

.. _transaction_log_num_write_buffers:

* ``transaction_log_num_write_buffers``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--transaction-log.num-write-buffers`

   Number of slots for in-memory write buffers (default=8).

.. _transaction_log_truncate_debug:

* ``transaction_log_truncate_debug``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--transaction-log.truncate-debug`

   DEBUGGING - Truncate transaction log

.. _transaction_log_use_replicator:

* ``transaction_log_use_replicator``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--transaction-log.use-replicator`

   Name of the replicator plugin to use (default='default_replicator')

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

.. _transaction_log_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _transaction_log_authors:

Authors
-------

Jay Pipes

.. _transaction_log_version:

Version
-------

This documentation applies to **transaction_log 0.1.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='transaction_log'

Changelog
---------

v0.1.1
^^^^^^
* First release.
