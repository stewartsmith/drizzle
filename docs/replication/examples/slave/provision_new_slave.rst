.. _provision_new_slave_example:

Provision New Slave
===================

:Synopsis: Provision a new slave from a backup of a master
:Replicator: :ref:`default_replicator`
:Applier: :ref:`slave`
:Version: :ref:`slave_1.1_drizzle_7.1`
:Authors: Marisa Plumb, Kent Bozlinski, Daniel Nichter

The basic formula for creating a new slave for an existing master is:

#. Make a backup of the master databases.
#. Record the max committed transaction ID of the master at the point the backup was made.
#. Restore the backup on the new slave.
#. Start the new slave from the recorded max committed transaction ID of the master.

.. program:: drizzledump

Steps #1 and #2 are performed using the :ref:`drizzledump` and its :option:`--single-transaction` option which prints a comment near the beginning of the dump output with the InnoDB transaction log metadata:

.. code-block:: bash

   master$ drizzledump --all-databases --single-transaction > master-backup.sql

   master$ grep SYS_REPLICATION_LOG master-backup.sql
   -- SYS_REPLICATION_LOG: COMMIT_ID = 3303, ID = 3500

The ``SYS_REPLICATION_LOG`` comment provides the replication log metadata needed to start a new slave. There are two values:

COMMIT_ID
   The commit sequence number recorded for the most recently executed transaction stored in the transaction log.  This value is used to determine proper commit order within the log.  ``ID``, the unique transaction identifier, cannot be used because it is assigned when the transaction is started, not when it is committed.

ID
   The unique transaction identifier associated with the most recently executed transaction stored in the transaction log.

For step #3, start the slave *without* the :ref:`slave_plugin` to prevent it from reading from the master until the backup is imported.  Then import the backup on the slave:

.. code-block:: bash

	slave$ drizzle < master-backup.sql

Stop the slave once the backup finishes importing.

For step #4, add the :ref:`max-commit-id option <slave_cfg_master_options>` to the :ref:`slave_config_file`:

.. code-block:: ini

   # Example of existing lines
   [master1]
   master-host=10.0.0.1
   master-user=slave
   master-pass=foo

   # Add this line
   max-commit-id=3303

The value for :ref:`max-commit-id <slave_cfg_master_options>` is the ``COMMIT_ID`` value from the ``SYS_REPLICATION_LOG`` comment in the master dump file (steps #1 and #2).  This value defines the committed transaction ID on the master from which the slave will start applying transactions.

Finally, start the slave *with* the :ref:`slave_plugin` and verify that replication is working (see :ref:`slave_admin`). 
