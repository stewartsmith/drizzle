.. _filtered_replicator:

Filtered Replicator
===================

The filtered replicator plugin, named ``filtered_replicator``, filters
replication events based on schema name or table name.  Regular expressions
can be used for the schema and table names.

.. _filtered_replicator_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=filtered_replicator

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`filtered_replicator_configuration` and :ref:`filtered_replicator_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _filtered_replicator_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about
specifying command line options.

.. program:: drizzled

.. option:: --filtered-replicator.filteredschemas ARG

   :Default: 
   :Variable: :ref:`filtered_replicator_filteredschemas <filtered_replicator_filteredschemas>`

   Comma-separated list of schemas to exclude from replication.

.. option:: --filtered-replicator.filteredtables ARG

   :Default: 
   :Variable: :ref:`filtered_replicator_filteredtables <filtered_replicator_filteredtables>`

   Comma-separated list of tables to exclude from replication.

.. option:: --filtered-replicator.schemaregex ARG

   :Default: 
   :Variable: :ref:`filtered_replicator_schemaregex <filtered_replicator_schemaregex>`

   Regular expression to apply to schemas to exclude from replication.

.. option:: --filtered-replicator.tableregex ARG

   :Default: 
   :Variable: :ref:`filtered_replicator_tableregex <filtered_replicator_tableregex>`

   Regular expression to apply to tables to exclude from replication.

.. _filtered_replicator_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _filtered_replicator_filteredschemas:

* ``filtered_replicator_filteredschemas``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--filtered-replicator.filteredschemas`

   Comma-separated list of schemas to exclude from replication.

.. _filtered_replicator_filteredtables:

* ``filtered_replicator_filteredtables``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--filtered-replicator.filteredtables`

   Comma-separated list of tables to exclude from replication.

.. _filtered_replicator_schemaregex:

* ``filtered_replicator_schemaregex``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--filtered-replicator.schemaregex`

   Regular expression to apply to schemas to exclude from replication.

.. _filtered_replicator_tableregex:

* ``filtered_replicator_tableregex``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--filtered-replicator.tableregex`

   Regular expression to apply to tables to exclude from replication.

.. _filtered_replicator_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _filtered_replicator_authors:

Authors
-------

Padraig O Sullivan

.. _filtered_replicator_version:

Version
-------

This documentation applies to **filtered_replicator 0.2**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='filtered_replicator'

Chagnelog
---------

v0.2
^^^^
* First release.
