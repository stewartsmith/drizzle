Schema Storage Engine
=====================

This implements the default file based Schema engine.

.. _schema_engine_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`schema_engine_configuration` and
:ref:`schema_engine_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=schema_engine

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _schema_engine_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. _schema_engine_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _schema_engine_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _schema_engine_authors:

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

