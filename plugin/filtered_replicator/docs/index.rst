*******************
Filtered Replicator
*******************

Description
###########

The Filtered Replicator plugin registers itself in the Drizzle kernel replication
stream process as a new replicator (see :ref:`replication_streams` for more
information). It provides a way to filter replication messages by schema name or
table name. Regular expressions can be used for the schema and table names.

To make the filtered replicator available, you must enable the plugin when the
server is started using the :option:`--plugin-add` option.

Configuration
#############

.. program:: drizzled

.. option:: --filtered-replicator.filteredschemas

   Comma-separated list of schema names to exclude from replication.

.. option:: --filtered-replicator.filteredtables

   Comma-separated list of table names to exclude from replication.

.. option:: --filtered-replicator.schemaregex

   Regular expression to apply to schemas to exclude from replication.

.. option:: --filtered-replicator.tableregex

   Regular expression to apply to tables to exclude from replication.

Examples
########

To prevent changes to the *foo* schema from being replicated::

  sbin/drizzled --plugin-add=filtered_replicator \
                --filtered-replicator.filteredschemas=foo

To prevent changes to any schema beginning with *test* from being replicated::

  sbin/drizzled --plugin-add=filtered_replicator \
                --filtered-replicator.schemaregex="test*"

To prevent changes to any table beginning with *junk* from being replicated::

  sbin/drizzled --plugin-add=filtered_replicator \
                --filtered-replicator.tableregex="junk*"
