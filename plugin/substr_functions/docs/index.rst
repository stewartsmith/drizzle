.. _substr_functions_plugin:

Substring Functions
===================

The ``substr_functions`` plugin provides these :doc:`/functions/overview`:

* :ref:`substr-function`

* :ref:`substring-index-function`

.. _substr_functions_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=substr_functions

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _substr_functions_authors:

Authors
-------

Stewart Smith

.. _substr_functions_version:

Version
-------

This documentation applies to **substr_functions 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='substr_functions'

Changelog
---------

v1.0
^^^^
* First release.
