CREATE SCHEMA
==============

CREATE SCHEMA enters a new schema into the current database. The schema name used must be distinct from the name of any existing schema.

.. code-block:: mysql

   CREATE SCHEMA [IF NOT EXISTS] schema_name
     [engine_options] ...

engine_options
--------------

You can specify the storage engine to use for creating the schema.

::

    engine_option [[,] engine_option] ...

engine_option
-------------

There are default settings for character sets and collations at four levels: server, database, table, and column. The COLLATE clause specifies the default database collation.

::

  [DEFAULT] COLLATE = collation_name
  { engine_specific }
