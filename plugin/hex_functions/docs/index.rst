.. _hex_functions_plugin:

Hex Functions
=============

The ``hex_functions`` plugin provides these :doc:`/functions/overview`:

* :ref:`hex-function`

* :ref:`unhex-function`

.. _hex_functions_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=hex_functions

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _hex_functions_authors:

Authors
-------

Stewart Smith

.. _hex_functions_version:

Version
-------

This documentation applies to **hex_functions 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='hex_functions'

Changelog
---------

v1.0
^^^^
* First release.
