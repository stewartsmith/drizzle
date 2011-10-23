.. _json_server_plugin:

JSON Server
===========

JSON HTTP interface.

.. _json_server_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=json_server

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`json_server_configuration` and :ref:`json_server_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _json_server_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --json-server.port ARG

   :Default: 8086
   :Variable: :ref:`json_server_port <json_server_port>`

   Port number to use for connection or 0 for default (port 8086) 

.. _json_server_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _json_server_port:

* ``json_server_port``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--json-server.port`

   Port number to use for connection or 0 for default (port 8086) 

.. _json_server_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _json_server_authors:

Authors
-------

Stewart Smith

.. _json_server_version:

Version
-------

This documentation applies to **json_server 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='json_server'

Changelog
---------

v0.1
^^^^
* First release.
