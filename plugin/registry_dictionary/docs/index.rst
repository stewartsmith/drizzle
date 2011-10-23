.. _registry_dictionary_plugin:

Registry Dictionary
===================

The :program:`registiry_dictionary` plugin provides the DATA_DICTIONARY.MODULES and DATA_DICTIONARY.PLUGINS tables.

.. _registry_dictionary_loading:

Loading
-------

This plugin is loaded by default.
To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=registry_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Authors
-------

Brian Aker

.. _registry_dictionary_version:

Version
-------

This documentation applies to **registry_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='registry_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
