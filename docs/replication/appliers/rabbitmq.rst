.. _rabbitmq_applier:

RabbitMQ Applier
================

The RabbitMQ applier plugin, named ``rabbitmq``, applies replication events to a `RabbitMQ <http://www.rabbitmq.com>`_ server.  This can be used to create advanced replication solutions, to visualize data, or to build triggers.

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=rabbitmq

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`rabbitmq_configuration` and :ref:`rabbitmq_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _rabbitmq_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about
specifying command line options.

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


Implementation Details
----------------------

* :program:`drizzled` will not sart if the rabbitmq server is not available.
* If the rabbitmq server goes away, the plugin will try to reconnect and resend the message 3 times, after that, the transaction is rolled back.

.. _rabbitmq_version:

Version
-------

This documentation applies to :ref:`rabbitmq_0.1_drizzle_7.0`.

To see which version of the ``rabbitmq`` plugin a Drizzle server is running,
execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='rabbitmq'

Changelog
---------

.. toctree::
   :maxdepth: 2

   rabbitmq/changelog

.. _rabbitmq_authors:

Authors
-------

Marcus Eriksson
