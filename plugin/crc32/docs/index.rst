.. _crc32_plugin:

CRC32 Function
==============

The ``crc32`` plugin provides these :doc:`/functions/overview`:

* :ref:`crc32-function`

.. _crc32_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=crc32

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _crc32_authors:

Authors
-------

Stewart Smith

.. _crc32_version:

Version
-------

This documentation applies to **crc32 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='crc32'

Changelog
---------

v1.0
^^^^
* First release.
