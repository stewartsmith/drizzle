.. _slave:

.. _slave_applier:

Slave Applier
=============

The slave applier plugin, named ``slave``, provides a native implementation
of replication between Drizzle servers by applying replication events from
multiple "master" servers to a "slave" server.  A slave is a server on which
the ``slave`` plugin is loaded, configured, and ran.  Slaves can connect to up
to ten masters at once over the network and pair with the
:ref:`default_replicator` on each them.  Slaves apply every replication
event from every master; conequently, they contain the same schemas and
data as their masters.

Replication chains are supported: slaves can also be masters and replicate
events to other slaves.  There is no limit to the number of levels in which
masters and slaves can be enchained.

Replication circles, or master-master replication, are not currently supported.
Replication will break on one of the masters if this is attempted.

The next two sections, :ref:`slave_configuration` and :ref:`slave_administration`,  contain necessary information for configuring and administering slave-based replication.  The third section, :ref:`slave_details`, covers technical details about how the ``slave`` plugin and slave-based replication work.

.. toctree::
   :maxdepth: 2

   slave/configuration
   slave/administration
   slave/details

.. _slave_version:

Version
-------

This documentation applies to :ref:`slave_1.1_drizzle_7.1`.

To see which version of the ``slave`` plugin a Drizzle server is running,
execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='slave'

Changelog
---------

.. toctree::
   :maxdepth: 2

   slave/changelog

.. _slave_authors:

Authors
-------

David Shrewsbury
   Original ``slave`` plugin code.  Multi-master code.

Daniel Nichter
   Documentation for :ref:`slave_1.1_drizzle_7.1`.
