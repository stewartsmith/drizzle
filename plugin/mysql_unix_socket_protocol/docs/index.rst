.. _mysql_unix_socket_protocol_plugin:

MySQL Unix Socket Protocol
==========================

:program:`mysql_unix_socket_protocol` implements the MySQL UNIX socket
protocol.

.. _mysql_unix_socket_protocol_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`mysql_unix_socket_protocol_configuration` and
:ref:`mysql_unix_socket_protocol_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=mysql_unix_socket_protocol

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _mysql_unix_socket_protocol_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --mysql-unix-socket-protocol.clobber 

   :Default: 
   :Variable: :ref:`mysql_unix_socket_protocol_clobber <mysql_unix_socket_protocol_clobber>`

   Clobber socket file if one is there already.

.. option:: --mysql-unix-socket-protocol.max-connections ARG

   :Default: 1000
   :Variable: :ref:`mysql_unix_socket_protocol_max_connections <mysql_unix_socket_protocol_max_connections>`

   Maximum simultaneous connections.

.. option:: --mysql-unix-socket-protocol.path ARG

   :Default: :file:`/tmp/mysql.socket`
   :Variable: :ref:`mysql_unix_socket_protocol_path <mysql_unix_socket_protocol_path>`

   Path used for MySQL UNIX Socket Protocol.

.. _mysql_unix_socket_protocol_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _mysql_unix_socket_protocol_clobber:

* ``mysql_unix_socket_protocol_clobber``

   :Scope: Global
   :Dynamic: No
   :Option:

   Clobber socket file if one is there already.

.. _mysql_unix_socket_protocol_max_connections:

* ``mysql_unix_socket_protocol_max_connections``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--mysql-unix-socket-protocol.max-connections`

   Maximum simultaneous connections.

.. _mysql_unix_socket_protocol_path:

* ``mysql_unix_socket_protocol_path``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--mysql-unix-socket-protocol.path`

   Path used for MySQL UNIX Socket Protocol.

.. _mysql_unix_socket_protocol_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _mysql_unix_socket_protocol_authors:

Authors
-------

Brian Aker

.. _mysql_unix_socket_protocol_version:

Version
-------

This documentation applies to **mysql_unix_socket_protocol 0.3**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='mysql_unix_socket_protocol'

Changelog
---------

v0.3
^^^^
* First Drizzle version.
