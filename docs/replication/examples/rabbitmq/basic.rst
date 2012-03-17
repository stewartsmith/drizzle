.. _basic_rabbitmq_example:

Basic RabbitMQ
==============

:Synopsis: Set up replication to a RabbitMQ server
:Replicator: :ref:`default_replicator`
:Applier: :ref:`rabbitmq_applier`
:Version: :ref:`rabbitmq_0.1_drizzle_7.0`
:Authors: Marcus Eriksson

First install a recent version of RabbitMQ, then install librabbitmq, the C library for talking to the RabbitMQ server:

.. code-block:: bash

   $ hg clone http://hg.rabbitmq.com/rabbitmq-codegen/
   $ hg clone http://hg.rabbitmq.com/rabbitmq-c/
   $ cd rabbitmq-c
   $ autoreconf -f -i
   $ ./configure
   $ make
   $ make install

Now you probably need to rebuild Drizzle since the :program:`rabbitmq` plugin is not built if librabbitmq is not installed.

Finally, start :program:`drizzled` like:

.. code-block:: bash

   sbin/drizzled                       \
      --daemon                         \
      --pid-file /var/run/drizzled.pid \
      --plugin-add rabbitmq            \
   >> /var/log/drizzled.log 2>&1

To verify that it works, you can start a generic rabbitmq listener from librabbitmq:

.. code-block:: bash

   $ amqp_listen localhost 5672 ReplicationExchange ReplicationRoutingKey

And you should see something like this when you do an INSERT/CREATE/.. (just not a select) in your newly built Drizzle instance::

   Result 0
   Frame type 1, channel 1
   Method AMQP_BASIC_DELIVER_METHOD
   Delivery 1, exchange ReplicationExchange routingkey ReplicationRoutingKey

   00000000: 0A 17 08 01 10 87 36 18 : F0 FA D9 99 FA F1 A7 02  ......6.........
   00000010: 20 99 81 DA 99 FA F1 A7 : 02 12 40 08 01 10 F2 FA   .........@.....
   00000020: D9 99 FA F1 A7 02 18 FC : FA D9 99 FA F1 A7 02 2A  ...............*
   00000030: 17 0A 06 0A 01 62 12 01 : 61 12 06 08 04 12 02 69  .....b..a......i
   00000040: 64 12 05 08 01 12 01 74 : 32 11 08 01 10 01 1A 0B  d......t2.......
   00000050: 0A 01 32 0A 02 61 61 10 : 00 10 00 20 01 28 01     ..2..aa.... .(.
   0000005F:
