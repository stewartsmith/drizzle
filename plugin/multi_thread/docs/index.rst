.. _multi_thread_plugin:

Multi-Thread Scheduler
======================

The :program:`multi_thread` plugin provides the low-level thread scheduler.

.. _multi_thread_loading:

Loading
-------

This plugin is loaded by default and it should not be unloaded because Drizzle will not start without this plugin.

.. _multi_thread_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --multi-thread.max-threads ARG

   :Default: 2048
   :Variable:

   Maximum number of user threads available.

.. _multi_thread_variables:

Variables
---------

The plugin does not register any variables.

.. _multi_thread_authors:

Authors
-------

Brian Aker

.. _multi_thread_version:

Version
-------

This documentation applies to **multi_thread 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='multi_thread'

Changelog
---------

v0.1
^^^^
* First release.
