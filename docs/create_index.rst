CREATE INDEX
============

.. code-block:: mysql

   CREATE [UNIQUE] INDEX index_name [USING {BTREE | HASH}] ON table_name (column_name [length] [ASC | DESC], ...);

An example:

.. code-block:: mysql

	CREATE INDEX table_1_index ON table_1 (a,b);

This would create an index on table_t named table_1_index that converged
columns a and b.

Fast index creation (where a storage engine can create or drop indexes
without copying and rebuilding the contents of the entire table) is
not implemented yet for Drizzle, but it is slated for the future.
