.. _simplest_master-slave_example:

Simplest Master-Slave
=====================

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

With the master running, you can optionally now create a backup of any databases to be imported on the new slave by using :doc:`/clients/drizzledump`. This example, however, assumes that we are starting with a fresh database with no data.

Slave Setup
-----------

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
