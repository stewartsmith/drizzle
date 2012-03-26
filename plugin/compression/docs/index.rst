.. _compression_plugin:

Compression Functions
=====================

The ``compression`` plugin provides these :doc:`/functions/overview`:

* :ref:`compress-function`

* :ref:`uncompress-function`

* :ref:`uncompressed-length-function`

.. _compression_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=compression

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _compression_authors:

Authors
-------

Stewart Smith

.. _compression_version:

Version
-------

This documentation applies to **compression 1.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='compression'

Changelog
---------

v1.1
^^^^
* First release.
