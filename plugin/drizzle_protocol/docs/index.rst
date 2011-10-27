.. _drizzle_protocol_plugin:

Drizzle Protocol
================

:program:`drizzle_protocol` implements the Drizzle network protocol.

.. _drizzle_protocol_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`drizzle_protocol_configuration` and
:ref:`drizzle_protocol_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=drizzle_protocol

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _drizzle_protocol_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --drizzle-protocol.bind-address ARG

   :Default: localhost
   :Variable: :ref:`drizzle_protocol_bind_address <drizzle_protocol_bind_address>`

   Address to bind to.

.. option:: --drizzle-protocol.buffer-length ARG

   :Default: 16384
   :Variable: :ref:`drizzle_protocol_buffer_length <drizzle_protocol_buffer_length>`

   Buffer length.

.. option:: --drizzle-protocol.connect-timeout ARG

   :Default: 10
   :Variable: :ref:`drizzle_protocol_connect_timeout <drizzle_protocol_connect_timeout>`

   Connect Timeout.

.. option:: --drizzle-protocol.max-connections ARG

   :Default: 1000
   :Variable: :ref:`drizzle_protocol_max_connections <drizzle_protocol_max_connections>`

   Maximum simultaneous connections.

.. option:: --drizzle-protocol.port ARG

   :Default: 4427
   :Variable: :ref:`drizzle_protocol_port <drizzle_protocol_port>`

   Port number to use for connection or 0 for default to with Drizzle/MySQL protocol.

.. option:: --drizzle-protocol.read-timeout ARG

   :Default: 30
   :Variable: :ref:`drizzle_protocol_read_timeout <drizzle_protocol_read_timeout>`

   Read Timeout.

.. option:: --drizzle-protocol.retry-count ARG

   :Default: 10
   :Variable: :ref:`drizzle_protocol_retry_count <drizzle_protocol_retry_count>`

   Retry Count.

.. option:: --drizzle-protocol.write-timeout ARG

   :Default: 60
   :Variable: :ref:`drizzle_protocol_write_timeout <drizzle_protocol_write_timeout>`

   Write Timeout.

.. _drizzle_protocol_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _drizzle_protocol_bind_address:

* ``drizzle_protocol_bind_address``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--drizzle-protocol.bind-address`

   Address to bind to.

.. _drizzle_protocol_buffer_length:

* ``drizzle_protocol_buffer_length``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--drizzle-protocol.buffer-length`

   Buffer length.

.. _drizzle_protocol_connect_timeout:

* ``drizzle_protocol_connect_timeout``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--drizzle-protocol.connect-timeout`

   Connect Timeout.

.. _drizzle_protocol_max_connections:

* ``drizzle_protocol_max_connections``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--drizzle-protocol.max-connections`

   Maximum simultaneous connections.

.. _drizzle_protocol_port:

* ``drizzle_protocol_port``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--drizzle-protocol.port`

   Port number to use for connection or 0 for default to with Drizzle/MySQL protocol.

.. _drizzle_protocol_read_timeout:

* ``drizzle_protocol_read_timeout``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--drizzle-protocol.read-timeout`

   Read Timeout.

.. _drizzle_protocol_retry_count:

* ``drizzle_protocol_retry_count``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--drizzle-protocol.retry-count`

   Retry Count.

.. _drizzle_protocol_write_timeout:

* ``drizzle_protocol_write_timeout``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--drizzle-protocol.write-timeout`

   Write Timeout.

.. _drizzle_protocol_authors:

Authors
-------

Brian Aker

.. _drizzle_protocol_version:

Version
-------

This documentation applies to **drizzle_protocol 0.3**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='drizzle_protocol'

Changelog
---------

v0.3
^^^^
* First release.
