How to use replication: An example
====================================

This page covers how to complete a simple replication setup between two Drizzle servers using the replication slave plugin. Some of the concepts are very similar to MySQL replication. This is the simplest of examples (single master, single slave), with additional explanation on how to provision a new slave into an existing setup.

Replication setup begins with making certain that both master and slave share the same version of Drizzle to avoid any potential incompatibility issues. Then you set up the master.

Master Setup
-------------

Setting up the master is the easiest step. The only real requirement for this step is to make sure that the master Drizzle database server is started with the --innodb.replication-log option. This will look something like:

    master> sbin/drizzled --datadir=$PWD/var --innodb.replication-log &


For more complex setups, you'll also want to consider using the --server-id option, but the default is fine for this example. That option is documented here.

With the master running, you can optionally now create a backup of any databases to be imported on the new slave. You would use :doc:`../../clients/drizzledump` to make such a backup. For the sake of simplicity, this example assumes that we are starting with a fresh database with no data.

Slave Setup
-------------

Once the master is running, the next step is to set up the slave. Starting the slave is almost as simple as starting the master. There are two Drizzle database server options required for the slave: --plugin-add=slave and --slave.config-file. This will look something like: ::

	slave> sbin/drizzled --datadir=$PWD/var \
                                    --plugin-add=slave \
                                    --slave.config-file=/tmp/slave.cfg &


These options tell the server to load the slave plugin, and then tell the slave plugin where to find the slave host configuration file. This configuration file has options to specify the master host and a few options to control how the slave operates. You can read more about the available configuration options in the replication slave plugin documentation. Below is a simple example: ::

	master-host = kodiak
	master-port = 3306
	master-user = kodiak_slave
	master-pass = my_password
	io-thread-sleep = 10
	applier-thread-sleep = 10

Most of these options have sensible defaults, and it should be pretty obvious what most of them are for. The plugin documentation referenced above describes them in more complete detail if you need more information.

So once you start the slave as described above, it will immediately connect to the master host specified in the configuration file and begin pulling events from the InnoDB-based transaction log. By default, a freshly provisioned slave will begin pulling from the beginning of this transaction log. Once all replication messages have been pulled from the master and stored locally on the slave host, the IO thread will sleep and periodically awaken to check for more messages. That's all fine and dandy for your first slave machine in a brand new replication setup, but how do you insert another slave host into an already existing replication architecture? I'm glad you asked!

Provisioning a New Slave Host
-------------------------------

Provisioning a new slave host very easy. The basic formula for creating a new slave host for an existing replication setup is:

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

Next, steps #3 and #4 must be completed to start the new slave. First, you must start the slave WITHOUT the replication slave plugin enabled. We don't want it reading from the master until the backup is imported. To start it without the plugin enabled, import your backup, then shutdown the server: ::

	slave> sbin/drizzled --datadir=$PWD/var &
	slave> drizzle < master.backup
	slave> drizzle --shutdown

Now that the backup is imported, restart the slave with the replication slave plugin enabled and use a new option, --slave.max-commit-id, to force the slave to begin reading the master's transaction log at the proper location:

	slave> sbin/drizzled --datadir=$PWD/var \
                                    --plugin-add=slave \
                                    --slave.config-file=/tmp/slave.cfg \
                                    --slave.max-commit-id=33426 &


We give the --slave.max-commit-id the value from the comment in the master dump file, which defines the maximum COMMIT_ID value (the latest transaction) represented by the slave's contents.

This is the full cycle for a simple replication example. Please see the other Drizzle slave plugin docs for more information on replication and configuration options.
