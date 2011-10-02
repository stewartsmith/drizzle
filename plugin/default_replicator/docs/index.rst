Default Replicator
==================

:program:`default_replicator` is a simple replicator which replicates all
write events to all appliers.

.. seealso:: :doc:`/replication`

.. _default_replicator_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`default_replicator_configuration` and
:ref:`default_replicator_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=default_replicator

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _default_replicator_configuration:

Configuration
-------------

This plugin does not have any command line options.

.. _default_replicator_variables:

Variables
---------

This plugin does not register any variables.

.. _default_replicator_authors:

Authors
-------

Jay Pipes

.. _default_replicator_version:

Version
-------

This documentation applies to **default_replicator 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='default_replicator'

Changelog
---------

v1.0
^^^^
* First release.
