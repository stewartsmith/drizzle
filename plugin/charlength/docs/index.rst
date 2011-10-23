.. _charlength_plugin:

CHAR_LENGTH Function
====================

The ``charlength`` plugin provides these :doc:`/functions/overview`:

* :ref:`char-length-function`

* :ref:`character-length-function`

.. _charlength_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=charlength

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _charlength_authors:

Authors
-------

Devananda van der Veen

.. _charlength_version:

Version
-------

This documentation applies to **charlength 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='charlength'

Changelog
---------

v1.0
^^^^
* First release.
