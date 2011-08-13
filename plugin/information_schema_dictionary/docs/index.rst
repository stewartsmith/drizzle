Information Schema Dictionary
=============================

Data Dictionary for ANSI information schema.

.. _information_schema_dictionary_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`information_schema_dictionary_configuration` and
:ref:`information_schema_dictionary_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=information_schema_dictionary

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _information_schema_dictionary_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. _information_schema_dictionary_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _information_schema_dictionary_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _information_schema_dictionary_authors:

Authors
-------

Brian Aker

.. _information_schema_dictionary_version:

Version
-------

This documentation applies to **information_schema_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='information_schema_dictionary'

