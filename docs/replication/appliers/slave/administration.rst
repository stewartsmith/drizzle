.. _slave_administration:

.. _slave_admin:

Slave Administration
********************

This page walks you through some common administration tasks when using
the replication slave plugin.

Monitoring the Slave
====================

On the slave server, the :ref:`slave` plugin uses two threads per master to read and apply replication events:

* An IO (or producer) thread
* An applier (or consumer) thread

An IO thread connects to a master and reads replication events from the :ref:`innodb_transaction_log` on the master.  An applier thread applies the replication events to the slave.  The status of these threads is stored in :ref:`sys_replication_tables`.

.. _sys_replication_tables:

sys_replication Tables
----------------------

The ``sys_replication`` schema on the slave has tables with information about the slave's threads:

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

io_state
^^^^^^^^

``sys_replication.io_state`` contains the status of IO threads:

.. code-block:: mysql

   drizzle> SELECT * FROM sys_replication.io_state;
   *************************** 1. row ***************************
   master_id: 1
      status: RUNNING
   error_msg: 

master_id
   ID (number) of the master to which the thread is connected.  The number corresponds to the the master number in the :ref:`slave_config_file`.

status
   Status of the IO thread: **RUNNING** or **STOPPED**.  If **STOPPED**, ``error_msg`` should contain an error message.

error_msg
   Error message explaining why the thread has **STOPPED**.

applier_state
^^^^^^^^^^^^^

``sys_replication.applier_stat`` contains the status of applier threads:

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
   Status of the applier thread: **RUNNING** or **STOPPED**.  If **STOPPED**, ``error_msg`` should contain an error message.

error_msg
   Error message explaining why the thread has **STOPPED**.

queue
^^^^^

``sys_replication.io_state`` contains replication events that have not yet been applied by the applier thread:

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

InnoDB Transaction Log
======================

The slave plugin uses the InnoDB transaction log (see
:ref:`innodb_transaction_log`) on the master to retrieve replication
messages. This transaction log, though stored as an internal table within
InnoDB, offers two different views to the table contents. Two tables in
the DATA_DICTIONARY schema provide the different views into the transaction
log: the SYS_REPLICATION_LOG table and the INNODB_REPLICATION_LOG table.

The SYS_REPLICATION_LOG table is read directly by the slave plugin.
This table is described as below::

  drizzle> SHOW CREATE TABLE data_dictionary.sys_replication_log\G
  *************************** 1. row ***************************
         Table: SYS_REPLICATION_LOG
  Create Table: CREATE TABLE `SYS_REPLICATION_LOG` (
    `ID` BIGINT,
    `SEGID` INT,
    `COMMIT_ID` BIGINT,
    `END_TIMESTAMP` BIGINT,
    `MESSAGE_LEN` INT,
    `MESSAGE` BLOB,
    PRIMARY KEY (`ID`,`SEGID`) USING BTREE,
    KEY `COMMIT_IDX` (`COMMIT_ID`,`ID`) USING BTREE
  ) ENGINE=InnoDB COLLATE = binary

The INNODB_REPLICATION_LOG is similar to the SYS_REPLICATION_LOG, the
main difference being that the Google Protobuffer message representing
the changed rows is converted to plain text before being output::

  drizzle> SHOW CREATE TABLE data_dictionary.innodb_replication_log\G
  *************************** 1. row ***************************
         Table: INNODB_REPLICATION_LOG
  Create Table: CREATE TABLE `INNODB_REPLICATION_LOG` (
    `TRANSACTION_ID` BIGINT NOT NULL,
    `TRANSACTION_SEGMENT_ID` BIGINT NOT NULL,
    `COMMIT_ID` BIGINT NOT NULL,
    `END_TIMESTAMP` BIGINT NOT NULL,
    `TRANSACTION_MESSAGE_STRING` TEXT COLLATE utf8_general_ci NOT NULL,
    `TRANSACTION_LENGTH` BIGINT NOT NULL
  ) ENGINE=FunctionEngine COLLATE = utf8_general_ci REPLICATE = FALSE

The INNODB_REPLICATION_LOG table is read-only due to the way it is
implemented. The SYS_REPLICATION_LOG table, on the other hand, allows you
to modify the contents of the transaction log. You would use this table
to trim the transaction log.

Transaction Log Maintenance
---------------------------

Currently, the InnoDB transaction log grows without bounds and is never
trimmed of unneeded entries. This can present a problem for long running
replication setups. You may trim the log manually, but you must make certain
to not remove any entries that are needed by slave servers.

Follow these steps to trim the InnoDB transaction without affecting slave
function:

#. Query each slave for the *last_applied_commit_id* value from the *sys_replication.applier_state* table.
#. Choose the **minimum** value obtained from step one. This will be the marker for the slave that is the furthest behind the master.
#. Using this marker value from the previous step, delete all entries from the master's transaction log that has a COMMIT_ID less than the marker value.

Below is an example of the steps defined above. First, step 1 and 2. Assume
that we have two slave hosts connected to the master (slave-1 and slave-2).
We need to query both to check their relationship with the master transaction
log::

  slave-1> SELECT last_applied_commit_id FROM sys_replication.applier_state\G
  *************************** 1. row ***************************
  last_applied_commit_id: 3000

  slave-2> SELECT last_applied_commit_id FROM sys_replication.applier_state\G
  *************************** 1. row ***************************
  last_applied_commit_id: 2877

We see that slave-2 has the smallest value for *last_applied_commit_id*. We
will use this value in the next step to trim the transaction log on the
master::

  master> DELETE FROM data_dictionary.sys_replication_log WHERE commit_id < 2877;

This will remove all old, unneeded entries from the InnoDB transaction log. Note that the SYS_REPLICATION_LOG table is used for this maintenance task.

Monitoring on a Master
======================

Slaves connected to a master are visible in the master's processlist, but they are not specially indicated.  If the :ref:`slave_user_account` uses special slave usernames, then slave connections can be selected like:

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

