.. program:: drizzled

.. _slave_configuration:

.. _slave_config:

Slave Configuration
*******************

The minimal steps for configuring a Drizzle
:ref:`replication stream <replication_streams>` using the
:ref:`slave` are:

#. Verifying that the :ref:`default_replicator` plugin is loaded on the masters
#. Enabling :option:`--innodb.replication-log` on the masters
#. Writing a :ref:`slave_config_file` for :option:`--slave.config-file`
#. Loading and configure the ``slave`` plugin on the slave

Masters
=======

A single slave can apply replication events from up to ten masters at once.
There are two requirements for each master:

#. :ref:`default_replicator`
#. :option:`--innodb.replication-log`

Each master must have the :ref:`default_replicator` plugin loaded because
the slave applier is hard-coded to pair only with this replicator.  The
:ref:`default_replicator` plugin loads by default, but to verify that it is
loaded on a master, execute:

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

A master can be started with other :ref:`replicators`, but only the
:ref:`default_replicator` is required for the ``slave`` plugin.

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

The :ref:`slave` will use a standard user account (username and password) to connect to the master.

.. _slave_config_file:

Slave Config File
=================

A slave config file is a plain text file that contains connection and
configuration options for each master.  At least one master must be
specifed, and masters must be numbered sequentially from 1 to 10.
The general syntax of a slave config file is:

.. code-block:: ini

 # comment
 [masterN]
 option=value

Options for each master begin with a ``[masterN]`` header where ``N``
is the sequentially numbered master, starting with 1.  Whitespace
before and after lines and around ``=`` (equal signs) is ignored.

The following options are permitted:

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

.. confval:: seconds-between-reconnects

   :Default: 30

   The number of seconds to wait between reconnect attempts when the master
   server becomes unreachable.

.. confval:: io-thread-sleep

   :Default: 5

   The number of seconds the IO (producer) thread sleeps between queries to the
   master for more replication events.

.. confval:: applier-thread-sleep

   :Default: 5

   The number of seconds the applier (consumer) thread sleeps between applying
   replication events from the local queue.

The simplest possible slave config file is:

.. code-block:: ini

   [master1]
   master-host=<master hostname>
   master-user=slave1

See :ref:`slave_examples` for complete, working examples.

slave Plugin
============

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
per-master in a :ref:`slave_config_file`.
Once a slave config file has been written, start Drizzle with the ``slave``
plugin like:

.. code-block:: bash

  $ drizzled --plugin-add slave --slave.config-file /etc/drizzled/slave.conf

See :ref:`slave_examples` for complete, working examples.

If the masters are configured properly and the slave config file is correct,
Drizzle should start without errors and it should be
possible to :ref:`administer the slave <slave_admin>` as described
in the next section.
