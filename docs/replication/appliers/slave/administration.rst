.. _slave_administration:

.. _slave_admin:

Slave Administration
********************

.. _slave_threads:

Slave Threads
=============

The :ref:`slave` plugin uses two threads per master to read and apply replication events:

* An IO (or producer) thread
* An applier (or consumer) thread

The IO thread connects to a master and reads replication events from the :ref:`slave_innodb_transaction_log` on the master.  The applier thread applies the replication events from the IO thread to the slave.  In other words, the IO thread produces replication events from the master, and the applier thread consumes and applies them to the slave.

There is currently no way to stop, start, or restart the threads.  Unless a replication error occurs, the threads should function continuously and remain in :ref:`RUNNING <slave_thread_running_status>` status.  If a replication error does occur, the Drizzle server must be stopped, the error fixed, and the server restarted.

.. _slave_thread_statuses:

Statuses
--------

Slave thread statuses are indicated by the ``status`` columns of the :ref:`sys_replication_tables`.  A slave thread status is always one of these values:

.. _slave_thread_running_status:

RUNNING
   The thread is working properly, applying replicaiton events.

.. _slave_thread_stopped_status:

STOPPED
   The thread has stopped due to an error; replication events are not being applied.

.. _sys_replication_tables:

sys_replication Tables
======================

The ``sys_replication`` schema on the slave has tables with information about the state of :ref:`slave_threads` and other replication-related information:

.. code-block:: mysql

   drizzle> SHOW TABLES FROM sys_replication;
   +---------------------------+
   | Tables_in_sys_replication |
   +---------------------------+
   | applier_state             | 
   | io_state                  | 
   | queue                     | 
   +---------------------------+

If the ``sys_replication`` table is not available, then the :ref:`slave` plugin failed to load.

.. _sys_replication_io_state:

io_state
--------

``sys_replication.io_state`` contains the state of IO threads, one per row:

.. code-block:: mysql

   drizzle> SELECT * FROM sys_replication.io_state;
   *************************** 1. row ***************************
   master_id: 1
      status: RUNNING
   error_msg: 

master_id
   ID (number) of the master to which the thread is connected.  The number corresponds to the the master number in the :ref:`slave_config_file`.

status
   :ref:`Status <slave_thread_statuses>` of the IO thread.

error_msg
   Error message explaining why the thread has :ref:`STOPPED <slave_thread_statuses>`.

.. _sys_replication_applier_state:

applier_state
-------------

``sys_replication.applier_state`` contains the state of applier threads, one per row:

.. code-block:: mysql

   drizzle> SELECT * FROM sys_replication.applier_state\G
   *************************** 1. row ***************************
                 master_id: 1
    last_applied_commit_id: 18
   originating_server_uuid: 9908C6AA-A982-4763-B9BA-4EF5F933D219
     originating_commit_id: 18
                    status: RUNNING
                 error_msg: 

master_id
   ID (number) of the master from which the thread is applying replication events.  The number corresponds to the the master number in the :ref:`slave_config_file`.

last_applied_commit_id
   Value of the ``COMMIT_ID`` from the master's replication log of the most recently executed transaction.  See definition of the data_dictionary.sys_replication_log table.

originating_server_uuid
   UUID of the :ref:`originating_server`.

originating_commit_id
   ``COMMIT_ID`` from the :ref:`originating_server`.

status
   :ref:`Status <slave_thread_statuses>` of the applier thread.

error_msg
   Error message explaining why the thread has :ref:`STOPPED <slave_thread_stopped_status>`.

.. _sys_replication_queue:

queue
-----

``sys_replication.io_state`` contains replication events that have not yet been applied by the applier thread, one per row:

.. code-block:: mysql

   drizzle> SELECT * FROM sys_replication.queue\G
   *************************** 1. row ***************************
                    trx_id: 925
                    seg_id: 1
              commit_order: 12
   originating_server_uuid: 9908C6AA-A982-4763-B9BA-4EF5F933D219
     originating_commit_id: 12
                       msg: transaction_context {
      server_id: 1
      transaction_id: 925
      start_timestamp: 1330211976689868
      end_timestamp: 1330211976689874
   }
   statement {
      type: DROP_SCHEMA
      start_timestamp: 1330211976689872
      end_timestamp: 1330211976689873
      drop_schema_statement {
         schema_name: "foo"
      }
   }
   segment_id: 1
   end_segment: true
                 master_id: 1

.. _slave_lag:

Slave Lag
=========

Slaves intentionally lag masters by at least :ref:`applier-thread-sleep <slave_cfg_common_options>` or :ref:`io-thread-sleep <slave_cfg_common_options>` seconds.  Since :ref:`replication_events` are transactions, *slave lag is the number of committed transactions behind a master*.  To determine a slave's lag, first query the last committed transaction ID on the master:

.. code-block:: mysql

   drizzle> SELECT MAX(COMMIT_ID) FROM DATA_DICTIONARY.SYS_REPLICATION_LOG;
   +----------------+
   | MAX(COMMIT_ID) |
   +----------------+
   |              6 | 
   +----------------+

Then query the last applied transaction ID on the slave:

.. code-block:: mysql

   drizzle> SELECT MAX(last_applied_commit_id) FROM sys_replication.applier_state;
   +-----------------------------+
   | MAX(last_applied_commit_id) |
   +-----------------------------+
   |                           6 | 
   +-----------------------------+

In this example, the slave is caught up to the master because both the last commited transaction ID on the master and the last applied transaction ID on the slave are 6.

If the last applied transaction ID on a slave is less than the last committed transaciton ID on a master, then the slave lags the master by *N transactions* (not seconds) where N is the difference of the two transaction ID values.

Master Connections
==================

Slaves connect to masters like normal users by specifying a username and password (see the :ref:`slave_cfg_master_options`).  Therefore, slave connections on a master are visible in the master's processlist and sessions, but they are not specially indicated.  If the :ref:`slave_user_account` uses slave-specific usernames like "slave1", then the slave connections can be viewed like:

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.PROCESSLIST  WHERE USERNAME LIKE 'slave%'\G
   *************************** 1. row ***************************
                ID: 2
          USERNAME: slave
              HOST: 127.0.0.1
                DB: NULL
           COMMAND: Sleep
              TIME: 0
             STATE: NULL
              INFO: NULL
   HAS_GLOBAL_LOCK: 0

The ``DATA_DICTIONARY.SESSIONS`` table can be queried similarly:

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.SESSIONS WHERE SESSION_USERNAME LIKE 'slave%'\G
   *************************** 1. row ***************************
         SESSION_ID: 2
   SESSION_USERNAME: slave1
       SESSION_HOST: 127.0.0.1
    SESSION_CATALOG: LOCAL
     SESSION_SCHEMA: NULL
            COMMAND: Sleep
              STATE: NULL
              QUERY: NULL
    HAS_GLOBAL_LOCK: 0
     IS_INTERACTIVE: 0
         IS_CONSOLE: 0

Or, slave connections can be viewed by specifying the slave server's hostname, like:

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.PROCESSLIST  WHERE HOSTNAME = '192.168.1.5'\G
   *************************** 1. row ***************************
                ID: 3
          USERNAME: slave
              HOST: 192.168.1.5
                DB: NULL
           COMMAND: Sleep
              TIME: 0
             STATE: NULL
              INFO: NULL
   HAS_GLOBAL_LOCK: 0

.. _slave_innodb_transaction_log:

InnoDB Transaction Log
======================

The :ref:`slave` requires the :ref:`innodb_transaction_log` on the master to retrieve replication events.  This transaction log is stored as an internal table within InnoDB, but there are two tables which provide access to its data:

* ``DATA_DICTIONARY.SYS_REPLICATION_LOG``
* ``DATA_DICTIONARY.INNODB_REPLICATION_LOG``

The :ref:`IO thread <slave_threads>` from a slave (which connects to a master) reads transactions (replicaiton events) directly from ``DATA_DICTIONARY.SYS_REPLICATION_LOG``.  The transaction messages are binary which makes the table data unreadable by most humans:

.. code-block:: mysql

   drizzle> SELECT * from DATA_DICTIONARY.SYS_REPLICATION_LOG\G
   *************************** 1. row ***************************
                        ID: 772
                     SEGID: 1
                 COMMIT_ID: 1
             END_TIMESTAMP: 1331841800496546
   ORIGINATING_SERVER_UUID: 98ECEA09-BA65-489D-9382-F8D15098B1AE
     ORIGINATING_COMMIT_ID: 1
               MESSAGE_LEN: 33
                   MESSAGE: ?????????

The last column, ``MESSAGE``, contains the actual transaction data that the client renders as question marks because the data is binary, not text.

The ``DATA_DICTIONARY.INNODB_REPLICATION_LOG`` table contains the same data as the ``DATA_DICTIONARY.SYS_REPLICATION_LOG`` table, but it converts the transaction data to text:

.. code-block:: mysql

   drizzle> SELECT * from DATA_DICTIONARY.INNODB_REPLICATION_LOG\G
   *************************** 1. row ***************************
               TRANSACTION_ID: 772
       TRANSACTION_SEGMENT_ID: 1
                    COMMIT_ID: 1
                END_TIMESTAMP: 1331841800496546
      ORIGINATING_SERVER_UUID: 98ECEA09-BA65-489D-9382-F8D15098B1AE
        ORIGINATING_COMMIT_ID: 1
   TRANSACTION_MESSAGE_STRING: transaction_context {
      server_id: 1
      transaction_id: 772
      start_timestamp: 1331841800496542
      end_timestamp: 1331841800496546
   }
   event {
      type: STARTUP
   }
   segment_id: 1
   end_segment: true
           TRANSACTION_LENGTH: 33

The ``TRANSACTION_MESSAGE_STRING`` column contains the text representation of the ``MESSAGE`` column from the ``DATA_DICTIONARY.SYS_REPLICATION_LOG`` table.


``DATA_DICTIONARY.INNODB_REPLICATION_LOG`` is read-only, but ``DATA_DICTIONARY.SYS_REPLICATION_LOG`` can be modified which allows the transaction log to be maintained, as described in the next section.

Transaction Log Maintenance
---------------------------

Currently, the InnoDB transaction log grows without bounds and old transactions are never deleted.  The InnoDB transaction log must be maintained manually by carefully deleting old transactions that are no longer needed from the ``DATA_DICTIONARY.SYS_REPLICATION_LOG`` table.

.. warning:: Care must be taken to avoid deleting transactions that slaves have not yet applied else data will be lost and replication will break.

Follow these steps to trim the InnoDB transaction log without affecting slave function:

#. Query each slave for the ``last_applied_commit_id`` value from the :ref:`sys_replication.applier_state <sys_replication_applier_state>` table.
#. Choose the **minimum** value obtained from step one. This is the marker value for the slave that is the furthest behind the master.
#. Using the marker value from the previous step, delete rows from ``DATA_DICTIONARY.SYS_REPLICATION_LOG`` that have a ``COMMIT_ID`` less than the marker value.

For example, if there are two slaves, query each one for the minimum ``last_applied_commit_id``:

.. code-block:: mysql

   slave1> SELECT last_applied_commit_id FROM sys_replicaiton.applier_state;
   +------------------------+
   | last_applied_commit_id |
   +------------------------+
   |                   3000 | 
   +------------------------+

   slave2> SELECT last_applied_commit_id FROM sys_replicaiton.applier_state;
   +------------------------+
   | last_applied_commit_id |
   +------------------------+
   |                   2877 | 
   +------------------------+

slave2 has the smallest value for ``last_applied_commit_id``, 2877, so this value is the marker for deleting records from the master's transaction log:

.. code-block:: mysql

  master> DELETE FROM DATA_DICTIONARY.SYS_REPLICATION_LOG
       -> WHERE COMMIT_ID < 2877;

This permanently deletes all old, unneeded records from the InnoDB transaction log.
