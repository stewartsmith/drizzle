.. _schema_engine_plugin:

Schema Engine
=============

The :program:`schema_engine` plugin provides the low-level schema system.

.. _schema_engine_loading:

Loading
-------

This plugin is loaded by default and it should not be unloaded unless you want to disable access to all schemas.

Dependencies
^^^^^^^^^^^^

* :doc:`/plugins/signal_handler/index`

Authors
-------

Brian Aker

.. _schema_engine_version:

Version
-------

This documentation applies to **schema_engine 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='schema_engine'

Changelog
---------

v1.0
^^^^
* First release.
