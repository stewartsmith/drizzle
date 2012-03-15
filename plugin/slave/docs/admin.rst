********************************
Replication Slave Administration
********************************

This page walks you through some common administration tasks when using
the replication slave plugin.

Monitoring the Master
#####################

Slave Connections
*****************

If you want to determine which slave machines are connected to your
master, use the *SHOW PROCESSLIST* command. Slave connections will show
up in the output of this command.

InnoDB Transaction Log
**********************

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

Monitoring the Slave
####################

The slave plugin has two types of threads doing all of the work:

* An IO (or producer) thread
* An applier (or consumer) thread

The status of each thread is stored in tables in the *sys_replication*
schema. The IO thread status is contained in the *io_state* table, and
the applier thread status is in the *applier_state* table. You may query
these tables just like any other table. For example::

  drizzle> SELECT * FROM sys_replication.io_state\G
  *************************** 1. row ***************************
     status: RUNNING
  error_msg: 

The above shows that the IO thread is **RUNNING**. If there had been
an error on the IO thread, the status value would be **STOPPED** and
the error_msg column would contain information about the error.

We can check the state of the applier thread in a similar manner::

  drizzle> SELECT * FROM sys_replication.applier_state\G
  *************************** 1. row ***************************
  last_applied_commit_id: 4
                  status: RUNNING
               error_msg: 

The status and error_msg columns are similar to the ones in the *io_state*
table. Also available is the last_applied_commit_id, which contains the
value of the COMMIT_ID from the master's replication log (see definition
of the data_dictionary.sys_replication_log table above) of the most
recently executed transaction.

Transaction Log Maintenance
###########################

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
