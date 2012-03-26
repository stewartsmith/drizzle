.. _sleep_plugin:

SLEEP Function
==============

The ``sleep`` plugin provides these :doc:`/functions/overview`:

* :ref:`sleep-function`

.. _sleep_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=sleep

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _sleep_authors:

Authors
-------

Patrick Galbraith

.. _sleep_version:

Version
-------

This documentation applies to **sleep 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='sleep'

Changelog
---------

v1.0
^^^^
* First release.
