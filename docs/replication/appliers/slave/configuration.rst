.. program:: drizzled

.. _slave_configuration:

.. _slave_config:

Slave Configuration
*******************

Configuring a Drizzle :ref:`replication stream <replication_streams>` using the :ref:`slave` requires:

#. Load a :ref:`replicator plugin <replicators>` on the masters
#. Enabling :option:`--innodb.replication-log` on the masters
#. Creating user accounts for the slaves on the masters
#. Writing a :ref:`slave_config_file` for :option:`--slave.config-file` on the slave
#. Loading and configure the ``slave`` plugin on the slave

The first three steps are performed on a master, and the last two steps are performed on a slave.

.. _configuring_a_master:

Configuring a Master
====================

A single slave can apply replication events from up to ten masters at once.
There are three requirements for each master:

#. :ref:`replicators`
#. :option:`--innodb.replication-log`
#. :ref:`slave_user_account`

A :ref:`replicator plugin <replicators>` must be loaded on each master.  The :ref:`default_replicator` is loaded by default.  To verify that it is loaded on a master, execute:

.. code-block:: sql

 drizzle> SELECT * FROM DATA_DICTIONARY.PLUGINS WHERE PLUGIN_NAME = 'default_replicator';
 +--------------------+-----------------------+-----------+--------------------+
 | PLUGIN_NAME        | PLUGIN_TYPE           | IS_ACTIVE | MODULE_NAME        |
 +--------------------+-----------------------+-----------+--------------------+
 | default_replicator | TransactionReplicator |         1 | default_replicator |
 +--------------------+-----------------------+-----------+--------------------+

If the plugin is not loaded, verify that the server was *not* started with
``--plugin-remove default_replicator``.  If it was, remove that option and
restart the server.

To use a different replicator, :ref:`configure Drizzle <configuring_drizzle>` to load the replicator plugin on startup, and specify the replicator name with :option:`--innodb.use-replicator`.

Each master must also be started with :option:`--innodb.replication-log`
to enable the :ref:`innodb_transaction_log` which is not enabled by default.
Therefore, Drizzle must be configured with this option at startup.
See :ref:`configuring_drizzle` for more information.  To verify that the
InnoDB replication log is active, execute:

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'innodb_replication_log';
   +------------------------+----------------+
   | VARIABLE_NAME          | VARIABLE_VALUE |
   +------------------------+----------------+
   | innodb_replication_log | ON             | 
   +------------------------+----------------+

   drizzle> SELECT * FROM DATA_DICTIONARY.INNODB_REPLICATION_LOG LIMIT 1;
   -- The query should return one row showing a replication event.

.. _slave_user_account:

Slave User Account
------------------

A user account is required on the master for slave connections, unless no :ref:`authentication` is used (which is highly inadvisable).  One user account can be used for all slaves, or individual user accounts can be used for each slave.  In either case, the user account credentials (username and password) for a master are specified in the :ref:`slave_config_file`.

:ref:`authorization` must be configured on the master to allow slave user accounts to access the ``DATA_DICTIONARY`` schema, else the :ref:`slave IO thread <slave_threads>` will fail to start on the slave:

.. code-block:: mysql

   drizzle> SELECT * FROM sys_replication.io_state\G
   *************************** 1. row ***************************
   master_id: 1
      status: STOPPED
   error_msg: Replication slave: Access denied for user 'slave' to schema 'data_dictionary'

The :ref:`sys_replication_tables` are discussed in :ref:`slave_admin`.

:ref:`authorization` cannot be used to filter which schemas or tables are replicated because slave connections do no access individual schemas and tables, they only access the :ref:`innodb_transaction_log`.  To filter which schemas or tables are replicated, configure the master to use a filtering replicator like :ref:`filtered_replicator`, as described above. 

.. _configuring_a_slave:

Configuring a Slave
===================

After :ref:`configuring_a_master`, configuring a slave requires only:

#. :ref:`slave_config_file`
#. :ref:`slave_plugin`

.. _slave_config_file:

Slave Config File
-----------------

A slave config file is a plain text file that contains connection and configuration options for each master.  At least one master must be specifed, and masters must be numbered sequentially from 1 to 10. The general syntax of a slave config file is:

.. code-block:: ini

 # comment
 common-option=value
 [masterN]
 master-specific-option=value

There are two types of options: common options which apply to all masters, and master-specific options which only apply to the preceding ``[masterN]`` header where ``N`` is the sequentially numbered master, starting with 1.  Whitespace
before and after lines and around ``=`` (equal signs) is ignored.

The simplest possible slave config file is:

.. code-block:: ini

   [master1]
   master-host=<master hostname>
   master-user=slave1

See :ref:`slave_examples` for complete, working examples.

.. _slave_cfg_common_options:

Common Options
--------------

These options must be specified first, before any ``[masterN]`` headers.

.. confval:: applier-thread-sleep

   :Default: 5

   The number of seconds the applier (consumer) thread sleeps between applying
   replication events from the local queue.

.. confval:: ignore-errors

   Ignore errors and continue applying replication events.  It is generally
   a bad idea to use this option!

.. confval:: io-thread-sleep

   :Default: 5

   The number of seconds the IO (producer) thread sleeps between queries to the
   master for more replication events.

.. confval:: seconds-between-reconnects

   :Default: 30

   The number of seconds to wait between reconnect attempts when the master
   server becomes unreachable.

.. _slave_cfg_master_options:

Master-specific Options
-----------------------

These options must be specified after a ``[masterN]`` header.

.. confval:: master-host

   Hostname/IP address of the master server.

.. confval:: master-port

   :Default: 3306

   Drizzle port used by the master server.

.. confval:: master-user

   Username to use for connecting to the master server.
   See :ref:`slave_user_account`.

.. confval:: master-pass

   Password associated with the username given by :confval:`master-user`.
   See :ref:`slave_user_account`.

.. program:: drizzledump

.. confval:: max-commit-id

   Maximum commit ID the slave is assumed to have applied from the master.
   This value will be used by the slave to determine where to begin retrieving
   replication events from the master transaction log. This option can be used
   to provision a new slave by setting it to the value output from the
   :ref:`drizzledump` when used with the :option:`--single-transaction` option.

.. confval:: max-reconnects

   :Default: 10

   The number of reconnection attempts the slave plugin will try if the
   master server becomes unreachable.

.. _slave_plugin:

slave Plugin
------------

A slave must load the ``slave`` plugin which is not loaded by default.
This plugin has only one option:

.. program:: drizzled

.. option:: --slave.config-file FILE

   :Default: :file:`BASEDIR/etc/slave.cfg`
   :Variable:

   Full path to a :ref:`slave_config_file`.
   By default, the plugin looks for a file named :file:`slave.cfg`
   in :file:`BASEDIR/etc/` where :file:`BASEDIR` is determined by
   :option:`--basedir`.

Since a slave can connect to multiple masters, all other options are set
in the :ref:`slave_config_file`.  Once a slave config file has been written, start Drizzle with the ``slave`` plugin like:

.. code-block:: bash

  $ drizzled --plugin-add slave --slave.config-file /etc/drizzled/slave.conf

If the masters are configured properly, and the slave config file is correct, and the slave plugin loads successfully, the :ref:`sys_replication_tables` will be accessible on the slave as described in the next topic, :ref:`slave_admin`.
