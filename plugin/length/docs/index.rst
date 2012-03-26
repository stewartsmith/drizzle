.. _length_plugin:

Length Functions
================

The ``length`` plugin provides these :doc:`/functions/overview`:

* :ref:`length-function`

* :ref:`octet-length-function`

.. _length_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=length

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _length_authors:

Authors
-------

Devananda van der Veen

.. _length_version:

Version
-------

This documentation applies to **length 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='length'

Changelog
---------

v1.0
^^^^
* First release.
