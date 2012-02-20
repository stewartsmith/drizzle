Getting Started
===============

.. topic:: Want to skip the details and setup Drizzle replicaiton as quickly as possible?

   Then start with the
   :ref:`Simplest Master-Slave example <simplest_master-slave_example>`.
   Otherwise, start here and read each section in sequence for a complete
   understanding of Drizzle replication.

Drizzle encodes replication events as
`Google Protocol Buffer <http://code.google.com/p/protobuf/>`_ (GPB) messages
and transmits them asynchronously through replication streams.  A replication
stream is a pair of one replicator and one applier.  The kernel encodes
replication events, sends them to all available replicators, which in turn send
them to their paired appliers.  For Drizzle, the replication process ends at
this point, but appliers are responsible for applying replication events to a
service: another Drizzle server (a slave), a message queue, etc.

.. code-block:: none

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

  * :ref:`Master-slave replication, including multi-master <slave_applier>`
  * :ref:`Replication to a RabbitMQ server <rabbitmq_applier>`
  * :ref:`Replication to a ZeroMQ socket <zeromq_applier>`

Master-slave is the most common replication system; it resembles other
database servers like MySQL. 

Unlike other database servers, the Drizzle kernel plays only a minimal role
in the replication process: it simply encodes and sends replication events
to available replicators.  Consequently, there are very few
:ref:`drizzled replication options <drizzled_replication_options>`, and
Drizzle replication is primarily configured by replicator and applier
plugin options.

In summary, Drizzle replication:

  * is asynchronous
  * encodes replication events as Google Buffer Protocol messages
  * sends replication events through replication streams (unique replicator-applier pairs)
  * only uses the kernel to encode and send replication events to avaiable replicators
  * is primarily implemented and configured by replicator and applier plugins
  * is extensible

Learned enough?  Ready to start using Drizzle replication?  Then jump to the
:ref:`replication examples <replication_examples>`.  Otherwise, continue
reading for more details about all the parts and processes of Drizzle
replication.

.. _replication_events:

Replication Events
------------------

Replication events are, in general, any SQL statements which change data or
schema objects on a server.  :doc:`/dml` queries are the primary cause of
replication events, but other statements like setting global variables or
altering schemas or tables may also cause replication events.

The Drizzle kernel sends every replication event to every applier (that is
to say, every loaded applier plugin), but appliers determine if replicaiton
events are sent to their paired appliers.  For example, a replicator may
filter certain replication events.  Likewise, appliers determine if (and how)
replication events are ultimately applied.

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

Replication stream are logical conduits created by pairing one replicator
with one applier.  As logical entities, replicaiton streams exist only inside
the :program:`drizzled` process and cannot be accessed externally.  However,
some appliers create or access ports or sockets which allows indirect access
to the replication stream.  Since replicators and appliers are implemented
by plugins, one could in theory program a custom applier or replicator to
provide a socket or port for direct access into the replication stream.
   
When :program:`drizlzed` starts, it creates replication streams automatically
based on which replicators are loaded and which appliers are loaded and
configured to use them.  For example, an applier plugin may be configured
to use a specific replicator, in which case :program:`drizzled` pairs the
applier to the specified replicator.  The user does not need to perform
special steps to create a replication stream.

Replication stream cannot be dynamically recreated; the user must stop
Drizzle, reconfigure the replicator or applier, and then restart Drizzle to
let it automatically recreate the new replication stream.
