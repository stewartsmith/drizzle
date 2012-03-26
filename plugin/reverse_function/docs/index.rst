.. _reverse_function_plugin:

REVERSE Function
================

The ``reverse_function`` plugin provides these :doc:`/functions/overview`:

* :ref:`reverse-function`

.. _reverse_function_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=reverse_function

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _reverse_function_authors:

Authors
-------

Stewart Smith

.. _reverse_function_version:

Version
-------

This documentation applies to **reverse_function 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='reverse_function'

Changelog
---------

v1.0
^^^^
* First release.
