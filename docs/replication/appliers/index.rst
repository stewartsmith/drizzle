.. _replication_appliers:

Appliers
========

Appliers are the other end point of a
:ref:`replication stream <replication_streams>`, the first being
:ref:`replicators <replication_replicators>`.  Appliers provide an
interface between replicators and a service.  The service can be anything:
another Drizzle server, a different database server, a message queue, etc.
Appliers receive replication events from replicators and apply them to the
service, although the term "apply" is used loosely.  Appliers can do anything
with replication events that provides useful behavior for the service.
For example, an applier may write the replicaiton event to a file on disk,
or it may send it over the network to some other service to be processed.

Appliers are implemented by plugins and specify the unique name of a
replicator with which Drizzle should pair it to create a replicaiton stream.
Most appliers can be configured to use a specific replicator, but some are
hard-coded to use a specific replicator.

Drizzle includes the following applier plugins:

.. toctree::
   :maxdepth: 1
   :glob:

   *
