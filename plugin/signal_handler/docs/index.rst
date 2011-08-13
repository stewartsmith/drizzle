Default Signal Handler
======================

Default Signal Handler.

.. _signal_handler_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`signal_handler_configuration` and
:ref:`signal_handler_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=signal_handler

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _signal_handler_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. _signal_handler_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _signal_handler_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _signal_handler_authors:

Authors
-------

Brian Aker

.. _signal_handler_version:

Version
-------

This documentation applies to **signal_handler 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='signal_handler'

