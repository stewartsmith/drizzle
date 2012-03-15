.. _signal_handler_plugin:

Signal Handler
==============

The :program:`signal_handler` plugin provides low-level signal handling.

.. _signal_handler_loading:

Loading
-------

This plugin is loaded by default and it should not be unloaded because the
:doc:`/plugins/schema_engine/index` plugin depends on it.

Authors
-------

Brian Aker

.. _signal_handler_version:

Version
-------

This documentation applies to **signal_handler 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='signal_handler'

Changelog
---------

v0.1
^^^^
* First release.
