Replication Dictionary
======================

Provides dictionary tables for replication system.

.. _replication_dictionary_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`replication_dictionary_configuration` and
:ref:`replication_dictionary_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=replication_dictionary

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _replication_dictionary_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. _replication_dictionary_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _replication_dictionary_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _replication_dictionary_authors:

Authors
-------

Jay Pipes

.. _replication_dictionary_version:

Version
-------

