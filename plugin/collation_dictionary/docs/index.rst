.. _collation_dictionary_plugin:

Character and Collation Dictionary
==================================

:program:`collation_dictionary` is a low-level plugin that provides
character set and collation dictionaries.

.. _collation_dictionary_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=collation_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Authors
-------

Brian Aker

.. _collation_dictionary_version:

Version
-------

This documentation applies to **collation_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='collation_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
