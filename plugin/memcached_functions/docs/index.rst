Memcached Functions
===================

Ronald Bradford.

.. _memcached_functions_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=memcached_functions

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`memcached_functions_configuration` and :ref:`memcached_functions_variables`.

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _memcached_functions_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. _memcached_functions_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _memcached_functions_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _memcached_functions_authors:

Authors
-------

Patrick Galbraith

.. _memcached_functions_version:

Version
-------

This documentation applies to **memcached_functions 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='memcached_functions'

