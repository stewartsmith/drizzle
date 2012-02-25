.. _slave:

.. _slave_applier:

Slave Applier
=============

The slave applier plugin provides a native implementation of replication
between Drizzle servers by applying replication events from multiple
:ref:`originating server <originating_server>`, or "masters", to another
Drizzle server, a "slave".  A slave is a server on which the slave applier
plugin is loaded, configured, and ran.  Slaves can connect to up to ten
masters at once over the network (TCP/IP) and pair with the
:ref:`default_replicator` on each them.  Slaves apply every replication
event from every master; conequently, they contain the same schemas and
data as their masters.

Replication chains are supported: slaves can also be masters and replicate
events to other slaves.  There is no limit to the number of levels in which
masters and slaves can be enchained.

Replication circles, or master-master replication, are not currently supported.
Replication will break on one of the masters if this is attempted.

Continue the following sections in order to learn how to use Drizzle slave
replication.

.. toctree::
   :maxdepth: 3

   slave/configuration
   slave/administration
   slave/examples
   slave/details

.. _slave_authors:

Authors
-------

:Code: David Shrewsbury
:Documentation: Daniel Nichter

.. _slave_version:

Version
-------

This documentation applies to **slave 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='slave'

Changelog
---------

v1.0
^^^^
* First release.
