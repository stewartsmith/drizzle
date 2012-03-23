.. _default_replicator:

Default Replicator
==================

The default replicator plugin, cleverly named ``default_replicator``,
does not modify or filter any replication events; it simply sends every
replication event it receives from the Drizzle kernel to every applier
with which it is paired.

:ref:`appliers` default to this replicator.

.. _default_replicator_loading:

Loading
-------

This plugin is loaded by default.   To stop the plugin from loading by
default, start :program:`drizzled` with::

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
