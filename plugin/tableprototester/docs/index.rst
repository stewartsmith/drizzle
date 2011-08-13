Table Proto Message Tester
==========================

Used to test rest of server with various table proto messages.

.. _tableprototester_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=tableprototester

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`tableprototester_configuration` and :ref:`tableprototester_variables`.

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _tableprototester_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. _tableprototester_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _tableprototester_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _tableprototester_authors:

Authors
-------

Stewart Smith

.. _tableprototester_version:

Version
-------

This documentation applies to **tableprototester 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='tableprototester'

