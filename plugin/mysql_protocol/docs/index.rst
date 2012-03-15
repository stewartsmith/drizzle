.. _mysql_protocol_plugin:

MySQL Protocol
==============

:program:`mysql_protocol` implements the MySQL network protocol.

.. _mysql_protocol_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`mysql_protocol_configuration` and
:ref:`mysql_protocol_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=mysql_protocol

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _mysql_protocol_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --mysql-protocol.bind-address ARG

   :Default: localhost
   :Variable: :ref:`mysql_protocol_bind_address <mysql_protocol_bind_address>`

   Address to bind to.

.. option:: --mysql-protocol.buffer-length ARG

   :Default: 16384
   :Variable: :ref:`mysql_protocol_buffer_length <mysql_protocol_buffer_length>`

   Buffer length.

.. option:: --mysql-protocol.connect-timeout ARG

   :Default: 10
   :Variable: :ref:`mysql_protocol_connect_timeout <mysql_protocol_connect_timeout>`

   Connect Timeout.

.. option:: --mysql-protocol.max-connections ARG

   :Default: 1000
   :Variable: :ref:`mysql_protocol_max_connections <mysql_protocol_max_connections>`

   Maximum simultaneous connections.

.. option:: --mysql-protocol.port ARG

   :Default: 3306
   :Variable: :ref:`mysql_protocol_port <mysql_protocol_port>`

   Port number to use for connection or 0 for default to with MySQL 

.. option:: --mysql-protocol.read-timeout ARG

   :Default: 30
   :Variable: :ref:`mysql_protocol_read_timeout <mysql_protocol_read_timeout>`

   Read Timeout.

.. option:: --mysql-protocol.retry-count ARG

   :Default: 10
   :Variable: :ref:`mysql_protocol_retry_count <mysql_protocol_retry_count>`

   Retry Count.

.. option:: --mysql-protocol.write-timeout ARG

   :Default: 60
   :Variable: :ref:`mysql_protocol_write_timeout <mysql_protocol_write_timeout>`

   Write Timeout.

.. _mysql_protocol_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _mysql_protocol_bind_address:

* ``mysql_protocol_bind_address``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--mysql-protocol.bind-address`

   Address to bind to.

.. _mysql_protocol_buffer_length:

* ``mysql_protocol_buffer_length``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--mysql-protocol.buffer-length`

   Buffer length.

.. _mysql_protocol_connect_timeout:

* ``mysql_protocol_connect_timeout``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--mysql-protocol.connect-timeout`

   Connect Timeout.

.. _mysql_protocol_max_connections:

* ``mysql_protocol_max_connections``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--mysql-protocol.max-connections`

   Maximum simultaneous connections.

.. _mysql_protocol_port:

* ``mysql_protocol_port``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--mysql-protocol.port`

   Port number to use for connection or 0 for default to with MySQL 

.. _mysql_protocol_read_timeout:

* ``mysql_protocol_read_timeout``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--mysql-protocol.read-timeout`

   Read Timeout.

.. _mysql_protocol_retry_count:

* ``mysql_protocol_retry_count``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--mysql-protocol.retry-count`

   Retry Count.

.. _mysql_protocol_write_timeout:

* ``mysql_protocol_write_timeout``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--mysql-protocol.write-timeout`

   Write Timeout.

.. _mysql_protocol_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _mysql_protocol_authors:

Authors
-------

Eric Day

.. _mysql_protocol_version:

Version
-------

This documentation applies to **mysql_protocol 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='mysql_protocol'

Changelog
---------

v0.1
^^^^
* First Drizzle version.
