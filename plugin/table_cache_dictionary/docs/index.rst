.. _table_cache_dictionary_plugin:

Table Cache Dictionary
======================

The :program:`table_cache_dictionary` plugin provides the DATA_DICTIONARY.TABLE_CACHE and DATA_DICTIONARY.TABLE_DEFINITION_CACHE tables.

.. _table_cache_dictionary_loading:

Loading
-------

This plugin is loaded by default.
To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=table_cache_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Authors
-------

Brian Aker

.. _table_cache_dictionary_version:

Version
-------

This documentation applies to **table_cache_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='table_cache_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
