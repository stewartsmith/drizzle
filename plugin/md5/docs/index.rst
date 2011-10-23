.. _md5_plugin:

MD5 Function
============

The ``md5`` plugin provides these :doc:`/functions/overview`:

* :ref:`md5-function`

.. _md5_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=md5

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _md5_authors:

Authors
-------

Stewart Smith

.. _md5_version:

Version
-------

This documentation applies to **md5 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='md5'

Changelog
---------

v1.0
^^^^
* First release.
