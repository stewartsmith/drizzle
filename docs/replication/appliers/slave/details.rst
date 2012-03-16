.. _slave_details:

Slave Details
=============

The following sections dive into the technical aspects of the :ref:`slave`.  In general, it should not be necessary to know this information to :ref:`configure <slave_configuration>` or :ref:`adminster <slave_admin>` a slave.  This information is useful for troubleshooting, developers, hackers, and those who wish to "look under the hood."

Slave Plugin Class
------------------

Although the documentation for the :ref:`slave` calls the plugin an applier, which implies that the plugin subclasses the TransactionApplier class, the :ref:`slave_plugin` is not in fact a TransactionApplier, it is a Daemon.  The :ref:`innodb_transaction_log` is a TransactionApplier which defaults to using with the :ref:`default_replicator` (see :file:`plugin/innobase/handler/replication_log.h`).
