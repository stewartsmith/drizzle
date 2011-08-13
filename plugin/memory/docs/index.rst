Memory Storage Engine
=====================

Hash based.

.. _memory_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`memory_configuration` and
:ref:`memory_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=memory

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _memory_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. _memory_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _memory_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _memory_authors:

Authors
-------

MySQL AB

.. _memory_version:

Version
-------

This documentation applies to **memory 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='memory'

