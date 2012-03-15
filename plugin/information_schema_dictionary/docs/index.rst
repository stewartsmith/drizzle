.. _information_schema_dictionary_plugin:

Information Schema Dictionary
=============================

The :program:`information_schema_dictionary` plugin provides the
`INFROMATION_SCHEMA` schema.

.. _information_schema_dictionary_loading:

Loading
-------

This plugin is loaded by default.  
To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=information_schema_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Authors
-------

Brian Aker

.. _information_schema_dictionary_version:

Version
-------

This documentation applies to **information_schema_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='information_schema_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
