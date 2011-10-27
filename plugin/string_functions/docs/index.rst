.. _string_functions_plugin:

String Functions
================

The ``string_functions`` plugin provides these :doc:`/functions/overview`:

* :ref:`elt-function`

* :ref:`format-function`

* :ref:`quote-function`

* :ref:`regex-function`

.. _string_functions_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=string_functions

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _string_functions_authors:

Authors
-------

Brian Aker

.. _string_functions_version:

Version
-------

This documentation applies to **string_functions 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='string_functions'

Changelog
---------

v1.0
^^^^
* First release.
