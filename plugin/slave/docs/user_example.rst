How to use replication: An example
====================================

A simple replication setup (using a single master and a single slave) between two Drizzle servers is done with the replication slave plugin. With Drizzle replication, you can also provision a new slave into an existing setup.

Replication setup begins with making certain that both master and slave share the same version of Drizzle to avoid any potential incompatibility issues.

Master Setup
-------------

Setting up the master is the first step. An important requirement is to start the master Drizzle database server with the --innodb.replication-log option, and a few other options in most circumstances. More options can be found in the options documentation. These are the most common options needed for a replication master. For example:

    master> usr/local/sbin/drizzled \
			--innodb.replication-log \
			--pid-file=/var/run/drizzled/drizzled.pid \
			--drizzle-protocol.bind-address=0.0.0.0 \
			--mysql-protocol.bind-address=0.0.0.0 \
			--daemon


Several options are required on most setups. They are set on Drizzle Startup with a --optionname. The most important ones are:


The InnoDB replication log must be running:

--innodb.replication-log


PID must be set:

--pid-file=/var/run/drizzled/drizzled.pid


the address binding for Drizzle's default port (4427):

--drizzle-protocol.bind-address=0.0.0.0


The address binding for systems replicating through MySQL's default port (3306):

--mysql-protocol.bind-address=0.0.0.0


Data Directory can be set other than default:

--datadir=$PWD/var


For more complex setups, the server id option may be appropriate to use:

--server-id


To run Drizzle in the background, thereby keeping the database running if the user logs out:

--daemon



With the master running, you can optionally now create a backup of any databases to be imported on the new slave by using :doc:`../../clients/drizzledump`. This example, however, assumes that we are starting with a fresh database with no data.

Slave Setup
-------------

Starting the slave is very similar to starting the master. There are two Drizzle database server options required for the slave: --plugin-add=slave and --slave.config-file. For example: ::

 	slave> /usr/local/sbin/drizzled \
                        --plugin-add=slave \
                        --slave.config-file=/usr/local/etc//slave.cfg  

A more typical startup will need more options:

	slave> /usr/local/sbin/drizzled \
			--plugin-add=slave \
			--slave.config-file=/usr/local/etc//slave.cfg \
			--pid-file=/var/run/drizzled/drizzled.pid \
                        --drizzle-protocol.bind-address=0.0.0.0 \
                        --mysql-protocol.bind-address=0.0.0.0 \
                        --daemon

Similar to the Master setup, there are a number of options that can be selected. Please see the Plugin Documentation for the relevent options.

These options tell the server to load the slave plugin, and then tell the slave plugin where to find the slave host configuration file. This configuration file has options to specify the master host and a few options to control how the slave operates. You can read more about the available configuration options in the replication slave plugin documentation. Below is a simple example: ::

	master-host = master.location.com
	master-port = 4427

Some options that can be set other than default, but are otherwise not necessary, are: 

	master-user = dino_slave
	master-pass = my_password
	io-thread-sleep = 10
	applier-thread-sleep = 10

The slave will immediately connect to the master host specified in the configuration file and begin pulling events from the InnoDB-based transaction log. By default, a freshly provisioned slave will begin pulling from the beginning of this transaction log. Once all replication messages have been pulled from the master and stored locally on the slave host, the IO thread will sleep and periodically awaken to check for more messages. This is straightforward for an initial replication setup. See below to learn about inserting another slave host into an already existing replication architecture.

Provisioning a New Slave Host
-------------------------------

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
