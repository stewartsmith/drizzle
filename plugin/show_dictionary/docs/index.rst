.. _show_dictionary_plugin:

SHOW Commands
=============

The :program:`show_dictionary` plugin provides these SHOW commands:

* SHOW COLUMNS
* SHOW INDEXES
* SHOW SCHEMAS
* SHOW [TEMPORARY] TABLES
* SHOW TABLE STATUS

.. _show_dictionary_loading:

Loading
-------

This plugin is loaded by default.
To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=show_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Authors
-------

Brian Aker

.. _show_dictionary_version:

Version
-------

This documentation applies to **show_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='show_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
