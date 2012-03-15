.. _uuid_function_plugin:

UUID Function
=============

The ``uuid_function`` plugin provides these :doc:`/functions/overview`:

* :ref:`uuid-function`

.. _uuid_function_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=uuid_function

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _uuid_function_authors:

Authors
-------

Stewart Smith

.. _uuid_function_version:

Version
-------

This documentation applies to **uuid_function 1.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='uuid_function'

Changelog
---------

v1.1
^^^^
* First release.
