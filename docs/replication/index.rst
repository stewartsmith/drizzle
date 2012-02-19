Getting Started
===============

.. topic:: New to Drizzle replication?  Want to skip the details and setup Drizzle replicaiton as quickly as possible?

   Then start with the
   :ref:`Simplest Master-Slave example <simplest_master-slave_example>`.
   Else, start here and read each section in sequence for a complete
   understanding of Drizzle replication.

Drizzle encodes replication events as
`Google Protocol Buffer <http://code.google.com/p/protobuf/>`_ (GPB) messages
and transmits them asynchronously through replication streams.  A replication
stream is a pair of one replicator and one applier.  The kernel encodes
replication events, sends them to all replicators, which in turn send them
to their paired appliers.  For Drizzle, the replication process ends at this
point, but appliers are responsible for applying replication events to a
service: another Drizzle server (a slave), a message queue, etc.::

  Drizzle                                                        External
  ==========================================================     ========
  User      Kernel                 Plugins
  =====     =================      =========================
                                   Replication Streams
                                   =========================
  event --> encode GBP(event) +--> replicator1 +--> applier1 --> service1
                              |                |
                              |                +--> applier2 --> service2
                              |
                              +--> replicator2 +--> applier3 --> service3

Replicators and appliers are implemented by plugins, so Drizzle replication
is extensible and varies depending on which plugins are used.  Drizzle
includes plugins to implement several replication systems:

  * Master-slave replication, including multi-master
  * Replication to a RabbitMQ server
  * Replication to a ZeroMQ socket

Master-slave is the most common replication system; it resembles other
database servers like MySQL. 

.. _replication_events:

Replication Events
------------------

Replication events are, in general, any SQL statements which change data or
schema objects on a server.  :doc:`/dml` queries are the primary cause of
replication events, but other statements like setting global variables or
altering schemas or tables may also cause replication events.

The Drizzle kernel sends every replication event to every applier, but
appliers determine if replicaiton events are sent to their paired
appliers.  For example, a replicator may filter certain replication events.
Likewise, appliers determine if (and how) replication events are ultimately
applied.

Replication events are not logged or saved by the Drizzle kernel or any
replicators that ship with Drizzle.  An applier may log replication events
that it receives from its replicator.

Every replication event is encapsulated as an atomic transaction, including
bulk and mutli-statement events.

Drizzle relinquishes all control of replication events once they enter a
replication stream.  Replicators and appliers are responsbile for handling
replication events correctly and efficiently.

.. _replication_streams:

Replication Streams
-------------------

Replication stream are *logical* conduits created by pairing one replicator
with one applier.  As logical entities, replicaiton streams exist only inside
the :program:`drizzled` process and cannot be accessed externally.  However,
some appliers create or access ports or sockets which allows indirect access
to the replication stream.  Since replicators and appliers are implemented
by plugins, one could in theory program a custom applier or replicator to
provide a socket or port for direct access into the replication stream.
   
:program:`drizlzed` creates replication streams *automatically* based on which
replicators are loaded and which appliers are configured to use them.  For
example, an applier plugin may be configured to use a specific replicator,
in which case :program:`drizzled` pairs the applier to the specified
replicator.  The user does not need to perform special steps to create
a replication stream.  Replication stream cannot be automatically recreated;
the user must stop Drizzle, reconfigure the replicator or applier, and then
restart Drizzle to let it automatically recreate the new replication stream.
