Multi-Thread Scheduler
======================

One Thread Per Session Scheduler.

.. _multi_thread_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`multi_thread_configuration` and
:ref:`multi_thread_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=multi_thread

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _multi_thread_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --multi-thread.max-threads ARG

   :Default: 2048
   :Variable:

   Maximum number of user threads available.

.. _multi_thread_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _multi_thread_examples:

Examples
--------

Sorry, there are no examples for this plugin.

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

