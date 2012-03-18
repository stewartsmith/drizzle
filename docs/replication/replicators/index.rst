.. _replicators:

.. _replication_replicators:

Replicators
===========

Replicators are one end point of a
:ref:`replication stream <replication_streams>` which provide an interface
between the Drizzle kernel and :ref:`appliers`.  The Drizzle kernel sends
replication events, encoded as Google Buffer Protocol message, to every
replicator that was loaded when Drizzled was stared.
Replicators implement specific behaviors by modifying or filtering
replication events before sending them to their paired appliers.
For example, a replicator may filter replication events from specific schemas
or tables.  There are no restrictions on how a replicator can modify
replication events.

Replicators are implemented by plugins and have unique names.  Most appliers
can be configured to use a specific replicator by specifying its unique name.

Drizzle includes the following replicator plugins:

.. toctree::
   :maxdepth: 1
   :glob:

   *
