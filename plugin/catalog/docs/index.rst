.. _catalog_plugin:

Catalog System
==============

The :program:`catalog` plugin provides the low-level catalog system.

.. _catalog_loading:

Loading
-------

This plugin is loaded by default and it should not be unloaded because
Drizzle will not start without this plugin.

.. _catalog_configuration:

Authors
-------

Brian Aker

.. _catalog_version:

Version
-------

This documentation applies to **catalog 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='catalog'

Changelog
---------

v0.1
^^^^
* First release.
