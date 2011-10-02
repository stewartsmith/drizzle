PrimeBase Media Stream Daemon
=============================

PrimeBase Technologies GmbH.

.. _pbms_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=pbms

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`pbms_configuration` and :ref:`pbms_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _pbms_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. _pbms_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _pbms_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _pbms_authors:

Authors
-------

Barry Leslie

.. _pbms_version:

Version
-------

This documentation applies to **pbms 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='pbms'

