.. _protocol_dictionary_plugin:

Protocol Dictionary
===================

The :program:`protocol_dictionary` plugin provides the
DATA_DICTIONARY.PROTOCOL_COUNTERS table.

.. _protocol_dictionary_loading:

Loading
-------

This plugin is loaded by default.
To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=protocol_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _protocol_dictionary_examples:

Examples
--------

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.PROTOCOL_COUNTERS;
   +----------------------------+--------------------+-------+
   | PROTOCOL                   | COUNTER            | VALUE |
   +----------------------------+--------------------+-------+
   | drizzle_protocol           | connection_count   |     2 | 
   | drizzle_protocol           | connected          |     1 | 
   | drizzle_protocol           | failed_connections |     0 | 
   | mysql_protocol             | connection_count   |     0 | 
   | mysql_protocol             | connected          |     0 | 
   | mysql_protocol             | failed_connections |     0 | 
   | mysql_unix_socket_protocol | connection_count   |     0 | 
   | mysql_unix_socket_protocol | connected          |     0 | 
   | mysql_unix_socket_protocol | failed_connections |     0 | 
   +----------------------------+--------------------+-------+

.. _protocol_dictionary_authors:

Authors
-------

Andrew Hutchings

.. _protocol_dictionary_version:

Version
-------

This documentation applies to **protocol_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='protocol_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
