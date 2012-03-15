.. _utility_functions_plugin:

Utility Functions
=================

The ``utility_functions`` plugin provides these :doc:`/functions/overview`:

* :ref:`assert-function`

* :ref:`bit-count-function`

* :ref:`catalog-function`

* :ref:`execute-function`

* :ref:`global-read-lock-function`

* :ref:`result-type-function`

* :ref:`kill-function`

* :ref:`database-function`

* :ref:`typeof-function`

* :ref:`user-function`

.. _utility_functions_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=utility_functions

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _utility_functions_authors:

Authors
-------

Brian Aker

.. _utility_functions_version:

Version
-------

This documentation applies to **utility_functions 1.4**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='utility_functions'

Changelog
---------

v1.4
^^^^
* First release.
