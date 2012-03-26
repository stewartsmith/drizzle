.. _benchmark_plugin:

BENCHMARK Function
==================

The ``benchmark`` plugin provides these :doc:`/functions/overview`:

* :ref:`benchmark-function`

.. _benchmark_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=benchmark

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _benchmark_authors:

Authors
-------

Devananda van der Veen

.. _benchmark_version:

Version
-------

This documentation applies to **benchmark 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='benchmark'

Changelog
---------

v1.0
^^^^
* First release.
