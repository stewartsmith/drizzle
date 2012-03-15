.. _version_plugin:

VERSION Function
================

The ``version`` plugin provides these :doc:`/functions/overview`:

* :ref:`version-function`

.. _version_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=version

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _version_authors:

Authors
-------

Devananda van der Veen

.. _version_version:

Version
-------

This documentation applies to **version 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='version'

Changelog
---------

v1.0
^^^^
* First release.
