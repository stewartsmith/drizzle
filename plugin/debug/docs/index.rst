.. _debug_plugin:

Debugging Functions
===================

The ``debug`` plugin provides these debugging functions:

* ``ASSERT_AND_CRASH``
* ``BACKTRACE``
* ``TRACE``

These functions are for Drizzle developers.

.. _debug_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=debug

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _debug_authors:

Authors
-------

Brian Aker

.. _debug_version:

Version
-------

This documentation applies to **debug 1.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='debug'

Changelog
---------

v1.1
^^^^
* First release.
