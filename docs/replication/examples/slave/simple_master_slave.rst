.. program:: drizzled

.. _simple_master_slave_example:

Simple Master-Slave
===================

:Synopsis: Set up one master and one slave, neither of which have existing data
:Replicator: :ref:`default_replicator`
:Applier: :ref:`slave`
:Version: :ref:`slave_1.1_drizzle_7.1`
:Authors: Daniel Nichter

This example uses new servers that are not yet configured and do not yet have any data.  If Drizzle was installed from a binary package or repository, then it may already be configured, in which case the following steps may not work or conflict with the existing configuration.  In any case, the principles in this example can be adapted to different configurations.

Master Setup
------------

First, temporarily start the master server with no :ref:`authentication` in order to create the user accounts.

.. code-block:: bash

   sbin/drizzled >> /var/log/drizzled.log 2>&1 &

Once Drizzle is running, create the user accounts:

.. code-block:: mysql

   drizzle> CREATE SCHEMA auth;
   
   drizzle> USE auth;
   
   drizzle> CREATE TABLE users (
         ->   user     VARCHAR(255) NOT NULL,
         ->   password VARCHAR(40),
         ->   UNIQUE INDEX user_idx (user)
         -> );

   drizzle> INSERT INTO auth.users (user, password)
         -> VALUES ('root', MYSQL_PASSWORD('foo')),
         ->        ('slave', MYSQL_PASSWORD('foo'));

   drizzle> shutdown;

Once Drizzle has stopped running, restart it with the :ref:`auth_schema_plugin` plugin and other plugins required for :ref:`configuring_a_master`:

.. code-block:: bash

   sbin/drizzled                                \
      --daemon                                  \
      --pid-file /var/run/drizzled/drizzled.pid \
      --plugin-remove auth_all                  \
      --plugin-add auth_schema                  \
      --innodb.replication-log                  \
   >> /var/log/drizzled.log 2>&1

See the options used to start Drizzle for more information:

* :option:`--daemon`
* :option:`--pid-file`
* :option:`--plugin-add`
* :option:`--innodb.replication-log`

Other options and :ref:`plugins` can be used if necessary.

Verify that the master is running and writing :ref:`replication_events` to the :ref:`innodb_transaction_log`:

.. code-block:: mysql

   $ drizzle --user=root --password=foo

   drizzle> SELECT ID FROM DATA_DICTIONARY.SYS_REPLICATION_LOG LIMIT 1;
   +------+
   | ID   |
   +------+
   |  772 | 
   +------+

The query should return a row.  This is the table from which slaves read replication events.

The master is now ready to replicate to slaves.

Slave Setup
-----------

The slave must be on a different sever than the master, else the two servers will have port conflicts.

Since the slave is new and has no data, it will replicate all data from the master, including the :ref:`auth_schema_plugin` table just created.

First, write a :ref:`slave_config_file`:

.. code-block:: ini

   [master1]
   master-host=10.0.0.1
   master-user=slave
   master-pass=foo

The :ref:`master-host option <slave_cfg_master_options>` must be set to the master server's address.  10.0.0.1 is just an example.

Save the config file in :file:`/etc/drizzle/slave.cfg`.

Then start the slave with the plugins required for :ref:`configuring_a_slave`:

.. code-block:: bash

   sbin/drizzled                                 \
      --daemon                                   \
      --pid-file /var/run/drizzled/drizzled.pid  \
      --plugin-add slave                         \
      --slave.config-file /etc/drizzle/slave.cfg \
   >> /var/log/drizzled.log 2>&1

Verify that the slave is running and applying replication events from the master:

.. code-block:: mysql

   $ drizzle --user=root --password=foo

   drizzle> SELECT * FROM sys_replication.io_state\G
   *************************** 1. row ***************************
   master_id: 1
      status: RUNNING
   error_msg: 

   drizzle> SELECT * FROM sys_replication.applier_state\G
   *************************** 1. row ***************************
                 master_id: 1
    last_applied_commit_id: 23
   originating_server_uuid: 98ECEA09-BA65-489D-9382-F8D15098B1AE
   originating_commit_id: 23
                  status: RUNNING
               error_msg: 

The column values will vary, but the important column is ``status`` :ref:`RUNNING <slave_thread_statuses>` for both tables.

After at least :ref:`applier-thread-sleep <slave_cfg_common_options>` or :ref:`io-thread-sleep <slave_cfg_common_options>` seconds, the :ref:`auth_schema_plugin` table should replicate from the master:

.. code-block:: mysql

   drizzle> SELECT * FROM auth.users;
   +-------+------------------------------------------+
   | user  | password                                 |
   +-------+------------------------------------------+
   | root  | F3A2A51A9B0F2BE2468926B4132313728C250DBF | 
   | slave | F3A2A51A9B0F2BE2468926B4132313728C250DBF | 
   +-------+------------------------------------------+

If not, then check the error log (:file:`/var/log/drizzled.log`) for errors.

Check the :ref:`slave_lag` to ensure that the slave has applied all committed transactions from the master.

Once the slave is fully caught up to the master, stop it, then start it again with the :ref:`auth_schema_plugin` plugin:

.. code-block:: bash

   sbin/drizzled                                 \
      --daemon                                   \
      --pid-file /var/run/drizzled/drizzled.pid  \
      --plugin-add slave                         \
      --slave.config-file /etc/drizzle/slave.cfg \
      --plugin-remove auth_all                   \
      --plugin-add auth_schema                   \
   >> /var/log/drizzled.log 2>&1

The root username and password should be required:

.. code-block:: bash
 
   $ drizzle --user=root --password=foo

The :ref:`sys_replication_tables` should still show that the :ref:`slave_threads` are :ref:`RUNNING <slave_thread_statuses>`.  Any changes on the master should replicate to the slave within a few seconds.  If any problems occur, consult the :ref:`slave` documentation or ask for :ref:`help`.  Else, congratulations: you are up and running with slave-based Drizzle replication!  Be sure to familiarize yourself with :ref:`slave_admin`.
