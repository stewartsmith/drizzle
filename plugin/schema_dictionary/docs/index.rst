.. _schema_dictionary_plugin:

Schema Dictionary
=================

The :program:`schema_dictionary` provides several DATA_DICTIONARY tables:

* COLUMNS
* FOREIGN_KEYS
* INDEXES
* INDEX_PARTS
* SCHEMAS
* TABLES
* TABLE_CONSTRAINTS

.. _schema_dictionary_loading:

Loading
-------

This plugin is loaded by default.
To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=schema_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Authors
-------

Brian Aker

.. _schema_dictionary_version:

Version
-------

This documentation applies to **schema_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='schema_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
