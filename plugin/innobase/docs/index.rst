Innobase
========

The Innobase plugin provides the InnoDB storage engine. It is almost identical
to the innodb_plugin, but adapted to Drizzle. We plan to move to having InnoDB
provided by the HailDB plugin, which will allow for easier maintenance and
upgrades.

InnoDB is the default storage engine for Drizzle. It is a fully transactional
MVCC storage engine.

innodb_plugin origins
---------------------

We maintain the Innobase plugin in Drizzle as a downstream project of the
innodb_plugin for MySQL. We try and keep it up to date with innodb_plugin
releases.

Differences from innodb_plugin
------------------------------

 * AUTO_INCREMENT behaves the standard way (as in MyISAM)
 * Supports four byte UTF-8 with the same index size

AIO support
-----------

InnoDB supports Linux native AIO when compiled on platforms that have the
libaio development files installed (typically a package called libaio-dev or
libaio-devel).  For more information on the advantages of this please see
http://blogs.innodb.com/wp/2010/04/innodb-performance-aio-linux/

To confirm that Linux native AIO is enabled execute this command:

.. code-block:: mysql

  show global variables like 'innodb_use_native_aio';

Compatibility with MySQL
------------------------

Although the innobase plugin is near identical to the innodb_plugin in MySQL,
the on disk formats are slightly incompatible (to allow for the same index
length for the four byte UTF-8 that Drizzle supports) and the table definitions
(FRM for MySQL, .dfe for Drizzle) are completely different. This means that you
cannot directly share InnoDB tablespaces between MySQL and Drizzle. Use the
drizzledump tool to migrate data from MySQL to Drizzle.

.. _innodb_transaction_log:

InnoDB Transaction Log
----------------------

The Innobase plugin provides a mechanism to store replication events in an
InnoDB table. When enabled, this transaction log can be accessed through
the SYS_REPLICATION_LOG and INNODB_REPLICATION_LOG tables in the DATA_DICTIONARY
schema.

To enable this transaction log, you must start the server with:

.. program:: drizzled

.. option::  --innodb.replication-log

It is not enabled by default.
