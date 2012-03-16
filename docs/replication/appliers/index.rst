.. _appliers:

.. _replication_appliers:

Appliers
========

Appliers are the other end point of a
:ref:`replication stream <replication_streams>`, the first being
:ref:`replicators`.  Appliers provide an
interface between replicators and a service.  The service can be anything:
another Drizzle server, a different database server, a message queue, etc.
Appliers receive replication events from replicators and apply them to the
service, although the term "apply" is used loosely.  Appliers can do anything
with replication events that provides useful behavior for the service.
For example, an applier may write the replicaiton event to a file on disk,
or it may send it over the network to some other service to be processed.

Appliers are implemented by plugins and specify the unique name of a replicator with which Drizzle should pair it to create a replicaiton stream.  Applier plugins default to using the :ref:`default_replicator`, but they can be configured to use another replicator.

Most applier plugins are loaded and ran on the :ref:`originating_server`,
but this is not a requirement.  For example, the :ref:`slave_applier` is
loaded on one server (the slave) and connects to and pairs with the
:ref:`default_replicator` on another server (the master).

Drizzle includes the following applier plugins:

.. toctree::
   :maxdepth: 1

   slave
   rabbitmq
   zeromq
