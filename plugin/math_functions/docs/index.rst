Math Functions
==============

The ``math_functions`` plugin provides these :doc:`/functions/overview`:

* :ref:`abs-function`

* :ref:`acos-function`

* :ref:`asin-function`

* :ref:`atan-function`

* :ref:`atan2-function`

* :ref:`cos-function`

* :ref:`log-function`

* :ref:`log2-function`

* :ref:`log10-function`

* :ref:`sin-function`

* :ref:`pow-function`

* :ref:`power-function`

* :ref:`ln-function`

* :ref:`sqrt-function`

* :ref:`ceil-function`

* :ref:`ceiling-function`

* :ref:`exp-function`

* :ref:`floor-function`

* :ref:`ord-function`

.. _math_functions_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=math_functions

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

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

