.. _memory_plugin:

Memory Storage Engine
=====================

The :program:`memory` plugin provides the Memory storage engine for in-memory,
hash-based tables.

.. _memory_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by default, start :program:`drizzled` with::

   --plugin-remove=memory

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

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

