.. _function_engine_plugin:

Function Engine
===============

:program:`function_engine` is a low-level plugin that provides the
interface for other function plugins.

.. warning::

   Drizzle depends on the :program:`function_engine` plugin.  If it is not
   loaded, most commands will not work and ``DATA_DICTIONARY`` and
   ``INFORMATION_SCHEMA`` will not be available.

.. _function_engine_loading:

Loading
-------

This plugin is loaded by default and it should not be unloaded.  See the
warning above.


Authors
-------

Brian Aker

.. _function_engine_version:

Version
-------

This documentation applies to **function_engine 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='function_engine'

Changelog
---------

v1.0
^^^^
* First release.
