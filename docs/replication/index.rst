Getting Started
===============

Drizzle encodes replication events as
`Google Protocol Buffer <http://code.google.com/p/protobuf/>`_ (GPB) messages
and transmits them asynchronously through replication streams.  A replication
stream is a pair of one replicator and one applier.  The kernel encodes
replication events, sends them to all replicators, which in turn send them
to their paired appliers::

  User      Kernel                 Plugins
  =====     =================      =========================

                                   Replication Streams
                                   =========================
  event --> encode GBP(event) +--> replicator1 +--> applier1
                              |                |
                              |                +--> applier2
                              |
                              +--> replicator2 +--> applier3

Replicators and appliers are implemented by plugins, so Drizzle replication
is extensible and varies depending on which plugins are used.  Drizzle
includes plugins to implement several replication systems:

  * Master-slave replication, including multi-master
  * Replication to a RabbitMQ server
  * Replication to a ZeroMQ socket

Master-slave is the most common replication system; it resembles other
database servers like MySQL.  To skip the details and start using Drizzle
replicaiton as quickly as possible, read the
:ref:`Master-Slave Quick Start <master-slave-quick-start>`.

.. _replication_streams:

Replication Streams
-------------------

A replication stream is a logical conduit created by pairing one replicator
with one applier.

Replicators
^^^^^^^^^^^

A replicator is a plugin that implements the TransactionReplicator
API.  Each replicator will be plugged into the kernel to receive the Google
Protobuf messages that are generated as the database is changed.  Ideally,
each registered replicator will transform or modify the messages it receives
to implement a specific behavior. For example, filtering by schema name.

Each registered replicator should have a unique name. The default replicator,
cleverly named **default_replicator**, does no transformation at all on the
replication messages.

Appliers
^^^^^^^^

A registered applier is a plugin that implements the TransactionApplier
API. Appliers are responsible for applying the replication messages that it
will receive from a registered replicator. The word "apply" is used loosely
here. An applier may do anything with the replication messages that provides
useful behavior. For example, an applier may simply write the messages to a
file on disk, or it may send the messages over the network to some other
service to be processed.

At the point of registration with the Drizzle kernel, each applier specifies
the name of a registered replicator that it should be attached to in order to
make the replication stream pair.

Replication Events
------------------

Replication events are anything that changes data on a server.  Events
are encoded and transmitted as Google Protobut messages.
