.. _provision_new_slave_example:

Provision New Slave
===================

:Synopsis: Provision a new slave from a backup of a master
:Replicator: :ref:`default_replicator`
:Applier: :ref:`slave`
:Difficulty: Nontrivial
:Use cases: New slaves, existing deployments

The basic formula for creating a new slave host for an existing replication setup is:

   1. Make a backup of the master databases.
   2. Record the state of the master transaction log at the point the backup was made.
   3. Restore the backup on the new slave machine.
   4. Start the new slave and tell it to begin reading the transaction log from the point recorded in #2.

Steps #1 and #2 are covered with the drizzledump client program. If you use the --single-transaction option to drizzledump, it will place a comment near the beginning of the dump output with the InnoDB transaction log metadata. For example: ::

	master> drizzledump --all-databases --single-transaction > master.backup
	master> head -1 master.backup
	-- SYS_REPLICATION_LOG: COMMIT_ID = 33426, ID = 35074

The SYS_REPLICATION_LOG line provides the replication log metadata needed when starting a new slave. It has two pieces of information:

* **COMMIT_ID**:  This value is the commit sequence number recorded for the most recently executed transaction stored in the transaction log. We can use this value to determine proper commit order within the log. The unique transaction ID cannot be used since that value is assigned when the transaction is started, not when it is committed.
* **ID**:  This is the unique transaction identifier associated with the most recently executed transaction stored in the transaction log.

Next, steps #3 and #4 must be completed to start the new slave. First, you must start the slave WITHOUT the replication slave plugin enabled, to prevent it from reading from the master until the backup is imported. To start it without the plugin enabled, import your backup, then shutdown the server: ::

	slave> sbin/drizzled --datadir=$PWD/var &
	slave> drizzle < master.backup
	slave> drizzle --shutdown

Now that the backup is imported, restart the slave with the replication slave plugin enabled and use a new option, --slave.max-commit-id, to force the slave to begin reading the master's transaction log at the proper location:

   slave> sbin/drizzled --datadir=$PWD/var \
                        --plugin-add=slave \
                        --slave.config-file=/user/local/etc/slave.cfg \
                        --slave.max-commit-id=33426 &

We give the --slave.max-commit-id the value from the comment in the master dump file, which defines the maximum COMMIT_ID value (the latest transaction) represented by the slave's contents.

This is the full cycle for a simple replication example. Please see the other Drizzle slave plugin docs for more information on replication and configuration options.
