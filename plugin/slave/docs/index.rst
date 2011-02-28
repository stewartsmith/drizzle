Replication Slave
=================

Description
-----------

The replication slave plugin provides a native implementation of replication
between two Drizzle processes.

This plugin requires a master that is running with the InnoDB replication log
enabled.

Configuration
-------------

Most of the options that can be used to control the replication slave plugin
can only be given in a configuration file. The only exception is the
**config-file** option which designates the location of this configuration
file.

**slave.config-file**

   Path to the replication slave configuration file.

The options below are read from the configuration file.

**master-host**

   Hostname/IP address of the master server.

**master-port**

   Drizzle port used by the master server. Default is 3306.

**master-user**

   Username to use for connecting to the master server.

**master-pass**

   Password associated with the username given by **master-user**.

**max-reconnects**

   The number of reconnection attempts the slave plugin will try if the
   master server becomes unreachable. Default is 10.

**seconds-between-reconnects**

   The number of seconds to wait between reconnect attempts when the master
   server becomes unreachable. Default is 30.

Implementation Details
----------------------

The replication slave plugin creates two worker threads, each accessing a
work queue (implemented as an InnoDB table) that contains the replication
events. This is a producer/consumer paradigm where one thread populates the
queue (the producer), and the other thread (the consumer) reads events from
the queue.

The producer thread (or I/O thread) is in charge of connecting to the master
server and pulling down replication events from the master's transaction
log and storing them locally in the slave queue. It is required that the
master use the InnoDB replication log (--innodb.replication-log=true).

The consumer thread (or applier thread) reads the replication events from
the local slave queue, applies them locally, and then deletes successfully
applied events from the queue.

Schemas and Tables
------------------

The slave plugin creates its own schema and set of tables to store its
metadata. It stores everything in the **sys_replication** schema. The
following are the tables that it will create:

**sys_replication.io_state**

   Stores metadata about the IO/producer thread.

**sys_replication.applier_state**

   Stores metadata about the applier/consumer thread.

**sys_replication.queue**

   The replication event queue.

