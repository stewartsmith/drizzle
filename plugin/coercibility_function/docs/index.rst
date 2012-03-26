.. _coercibility_function_plugin:

COERCIBILITY Function
=====================

The ``coercibility_function`` plugin provides these :doc:`/functions/overview`:

* :ref:`coercibility-function`

.. _coercibility_function_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=coercibility_function

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _coercibility_function_authors:

Authors
-------

Andrew Hutchings

.. _coercibility_function_version:

Version
-------

This documentation applies to **coercibility_function 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='coercibility_function'

Changelog
---------

v1.0
^^^^
* First release.
