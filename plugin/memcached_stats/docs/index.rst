.. _memcached_stats_plugin:

Memcached Statistics
====================

The :program:`memcached_stats` plugin provides `memcached <http://memcached.org/>`_ statistics as INFORMATION_SCHEMA tables.

.. _memcached_stats_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=memcached_stats

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`memcached_stats_configuration` and :ref:`memcached_stats_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _memcached_stats_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --memcached-stats.servers ARG

   :Default: 
   :Variable:

   List of memcached servers.

.. _memcached_stats_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _memcached_stats_servers:

* ``memcached_stats_servers``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--memcached-stats.servers`

   Memcached servers.

.. _memcached_stats_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _memcached_stats_authors:

Authors
-------

Padraig O'Sullivan

.. _memcached_stats_version:

Version
-------

This documentation applies to **memcached_stats 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='memcached_stats'

Changelog
---------

v1.0
^^^^
* First release.
