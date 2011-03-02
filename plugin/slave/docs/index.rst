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
:option:`--slave.config-file` option which designates the location of this configuration
file.

.. program:: drizzled

.. option:: --slave.config-file=arg

   Path to the replication slave configuration file. By default, the
   plugin will look for a file named `slave.cfg` in the `etc` directory
   of the Drizzle installation. If you want to specify a different path or
   configuration file name, it is best to specify a full path to the
   file. The relative path used by plugins is within the :option:`--datadir`
   directory, so a full path is recommended.

The options below are read from the configuration file.

.. confval:: master-host

   Hostname/IP address of the master server.

.. confval:: master-port

   Drizzle port used by the master server. Default is 3306.

.. confval:: master-user

   Username to use for connecting to the master server.

.. confval:: master-pass

   Password associated with the username given by :confval:`master-user`.

.. confval:: max-reconnects

   The number of reconnection attempts the slave plugin will try if the
   master server becomes unreachable. Default is 10.

.. confval:: seconds-between-reconnects

   The number of seconds to wait between reconnect attempts when the master
   server becomes unreachable. Default is 30.

.. confval:: io-thread-sleep

   The number of seconds the IO (producer) thread sleeps between queries to the
   master for more replication events. Default is 5.

.. confval:: applier-thread-sleep

   The number of seconds the applier (consumer) thread sleeps between applying
   replication events from the local queue. Default is 5.

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
master use the InnoDB replication log (:option:`--innodb.replication-log <drizzled --innodb.replication-log>`).

The consumer thread (or applier thread) reads the replication events from
the local slave queue, applies them locally, and then deletes successfully
applied events from the queue.

Schemas and Tables
------------------

The slave plugin creates its own schema and set of tables to store its
metadata. It stores everything in the **sys_replication** schema. The
following are the tables that it will create:

.. dbtable:: sys_replication.io_state

   Stores metadata about the IO/producer thread.

.. dbtable:: sys_replication.applier_state

   Stores metadata about the applier/consumer thread.

.. dbtable:: sys_replication.queue

   The replication event queue.

