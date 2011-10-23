.. _trigger_dictionary_plugin:

Trigger Dictionary
==================

The :program:`trigger_dictionary` provides the DATA_DICTIONARY.EVENT_OBSERVERS table which lists loaded event observer plugins.

.. _trigger_dictionary_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=trigger_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Examples
--------


If the :doc:`/plugins/query_log/index` plugin is loaded, then the
DATA_DICTIONARY.EVENT_OBSERVERS table should list it:

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.EVENT_OBSERVERS;
   +---------------------+
   | EVENT_OBSERVER_NAME |
   +---------------------+
   | query_log           | 
   +---------------------+

.. _trigger_dictionary_authors:

Authors
-------

Brian Aker

.. _trigger_dictionary_version:

Version
-------

This documentation applies to **trigger_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='trigger_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
