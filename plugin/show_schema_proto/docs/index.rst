.. _show_schema_proto_plugin:

SHOW_SCHEMA_PROTO Function
==========================

The ``show_schema_proto`` plugin provides these :doc:`/functions/overview`:

* :ref:`show-schema-proto-function`

.. _show_schema_proto_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=show_schema_proto

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _show_schema_proto_authors:

Authors
-------

Stewart Smith

.. _show_schema_proto_version:

Version
-------

This documentation applies to **show_schema_proto 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='show_schema_proto'

Changelog
---------

v1.0
^^^^
* First release.
