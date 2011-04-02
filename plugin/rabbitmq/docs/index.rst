RabbitMQ Integration
======================

It is possible to replicate transactions directly to a `RabbitMQ <http://www.rabbitmq.org>`_ server from drizzle, this could be used to create advanced replication solutions, to visualize data, or to build triggers. For example, `RabbitReplication <http://www.rabbitreplication.org>`_ has been built to consume the messages from rabbitmq, transform them, and persist the data in drizzle, mysql, many nosql stores and even replicating directly to websockets for data visualization.

Getting started
-----------------------
First install a recent version of RabbitMQ, then install librabbitmq, this is the c library for talking to the RabbitMQ server:

.. code-block:: bash

         $ hg clone http://hg.rabbitmq.com/rabbitmq-codegen/
	 $ hg clone http://hg.rabbitmq.com/rabbitmq-c/
	 $ cd rabbitmq-c
	 $ autoreconf -f -i
	 $ ./configure
	 $ make
	 $ make install

Now you probably need to rebuild drizzle (the rabbitmq plugin is not built if librabbitmq is not installed), see :ref:`dependencies`.

To start drizzled you need to add the replication plugins, start drizzled7 something like this (you probably want to add datadir etc as well, but these are the minimum):

.. code-block:: bash

         $ sbin/drizzled7 --plugin-add rabbitmq,default-replicator --rabbitmq.use-replicator default

To verify that it works, you can start a generic rabbitmq listener from librabbitmq:

.. code-block:: bash

         $ ./amqp_listen localhost 5672 ReplicationExchange ReplicationRoutingKey

And you should see something like this when you do an INSERT/CREATE/.. (just not a select) in your newly built drizzle instance:

.. code-block:: bash
      
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

It is possible to configure what `exchange <http://www.rabbitmq.com/faq.html#managing-concepts-exchanges>`_ and various other settings, these are the available config parameters:

.. code-block:: bash

   --rabbitmq.host arg (=localhost)                    Host name to connect to
   --rabbitmq.port arg (=5672)                         Port to connect to
   --rabbitmq.virtualhost arg (=/)                     RabbitMQ virtualhost
   --rabbitmq.username arg (=guest)                    RabbitMQ username
   --rabbitmq.password arg (=guest)                    RabbitMQ password
   --rabbitmq.use-replicator arg (=default_replicator) Name of the replicator 
   --rabbitmq.exchange arg (=ReplicationExchange)      Name of RabbitMQ exchange
   --rabbitmq.routingkey arg (=ReplicationRoutingKey)  Name of RabbitMQ routing 
   

Implementation details
-----------------------

* If the rabbitmq server is not available when starting drizzled, drizzled will not start
* If the rabbitmq server goes away, the plugin will try to reconnect and resend the message 3 times, after that, the transaction is rolled back

