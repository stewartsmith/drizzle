.. _memcached_functions_plugin:

Memcached Functions
===================

The :program:`memcahed_functions` plugin provide functions for accessing
a `memcached <http://memcached.org/>`_ server.

.. _memcached_functions_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=memcached_functions

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Examples
--------

Sorry, there are no examples for this plugin.

.. _memcached_functions_authors:

Authors
-------

Patrick Galbraith, Ronald Bradford, Padraig O'Sullivan

.. _memcached_functions_version:

Version
-------

This documentation applies to **memcached_functions 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='memcached_functions'

Changelog
---------

v0.1
^^^^
* First release.
