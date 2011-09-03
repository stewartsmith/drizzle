RabbitMQ Integration
======================

It is possible to replicate transactions directly to a `RabbitMQ <http://www.rabbitmq.org>`_ server from drizzle, this could be used to create advanced replication solutions, to visualize data, or to build triggers. For example, `RabbitReplication <http://www.rabbitreplication.org>`_ has been built to consume the messages from rabbitmq, transform them, and persist the data in drizzle, mysql, many nosql stores and even replicating directly to websockets for data visualization.

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=rabbitmq

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`rabbitmq_configuration` and :ref:`rabbitmq_variables`.

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _rabbitmq_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --rabbitmq.exchange ARG

   :Default: ReplicationExchange
   :Variable: :ref:`rabbitmq_exchange <rabbitmq_exchange>`

   Name of RabbitMQ exchange to publish to

.. option:: --rabbitmq.host ARG

   :Default: localhost
   :Variable: :ref:`rabbitmq_host <rabbitmq_host>`

   Host name to connect to

.. option:: --rabbitmq.password ARG

   :Default: guest
   :Variable: :ref:`rabbitmq_password <rabbitmq_password>`

   RabbitMQ password

.. option:: --rabbitmq.port ARG

   :Default: 5672
   :Variable: :ref:`rabbitmq_port <rabbitmq_port>`

   Port to connect to

.. option:: --rabbitmq.routingkey ARG

   :Default: ReplicationRoutingKey
   :Variable: :ref:`rabbitmq_routingkey <rabbitmq_routingkey>`

   Name of RabbitMQ routing key to use

.. option:: --rabbitmq.use-replicator ARG

   :Default: default_replicator
   :Variable:

   Name of the replicator plugin to use (default='default_replicator')

.. option:: --rabbitmq.username ARG

   :Default: guest
   :Variable: :ref:`rabbitmq_username <rabbitmq_username>`

   RabbitMQ username

.. option:: --rabbitmq.virtualhost ARG

   :Default: /
   :Variable: :ref:`rabbitmq_virtualhost <rabbitmq_virtualhost>`

   RabbitMQ virtualhost

.. _rabbitmq_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _rabbitmq_exchange:

* ``rabbitmq_exchange``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--rabbitmq.exchange`

   Name of RabbitMQ exchange to publish to

.. _rabbitmq_host:

* ``rabbitmq_host``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--rabbitmq.host`

   Host name to connect to

.. _rabbitmq_password:

* ``rabbitmq_password``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--rabbitmq.password`

   RabbitMQ password

.. _rabbitmq_port:

* ``rabbitmq_port``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--rabbitmq.port`

   Port to connect to

.. _rabbitmq_routingkey:

* ``rabbitmq_routingkey``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--rabbitmq.routingkey`

   Name of RabbitMQ routing key to use

.. _rabbitmq_username:

* ``rabbitmq_username``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--rabbitmq.username`

   RabbitMQ username

.. _rabbitmq_virtualhost:

* ``rabbitmq_virtualhost``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--rabbitmq.virtualhost`

   RabbitMQ virtualhost

.. _rabbitmq_examples:

Examples
--------

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

   sbin/drizzled --plugin-add rabbitmq,default-replicator \
                 --rabbitmq.use-replicator default

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

Implementation Details
----------------------

* :program:`drizzled` will not sart if the rabbitmq server is not available.
* If the rabbitmq server goes away, the plugin will try to reconnect and resend the message 3 times, after that, the transaction is rolled back.

.. _rabbitmq_authors:

Authors
-------

Marcus Eriksson

.. _rabbitmq_version:

Version
-------

This documentation applies to **rabbitmq 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='rabbitmq'

Changelog
---------

v0.1
^^^^
* First release.
