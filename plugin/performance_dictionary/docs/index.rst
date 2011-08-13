Performance Dictionary
======================

Data Dictionary for performance related table functions.

.. _performance_dictionary_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=performance_dictionary

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`performance_dictionary_configuration` and :ref:`performance_dictionary_variables`.

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _performance_dictionary_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. _performance_dictionary_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _performance_dictionary_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _performance_dictionary_authors:

Authors
-------

Brian Aker

.. _performance_dictionary_version:

Version
-------

This documentation applies to **performance_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='performance_dictionary'

