.. _math_functions_plugin:

Math Functions
==============

The ``math_functions`` plugin provides :ref:`mathematical functions <math_functions>`.

.. _math_functions_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=math_functions

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _math_functions_authors:

Authors
-------

Brian Aker

.. _math_functions_version:

Version
-------

This documentation applies to **math_functions 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='math_functions'

Changelog
---------

v1.0
^^^^
* First release.
