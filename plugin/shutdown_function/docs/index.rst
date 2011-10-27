.. _shutdown_function_plugin:

SHUTDOWN Function
=================

The ``shutdown_function`` plugin provides these :doc:`/functions/overview`:

* :ref:`shutdown-function`

.. _shutdown_function_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=shutdown_function

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _shutdown_function_authors:

Authors
-------

Brian Aker

.. _shutdown_function_version:

Version
-------

This documentation applies to **shutdown_function 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='shutdown_function'

Changelog
---------

v1.0
^^^^
* First release.
