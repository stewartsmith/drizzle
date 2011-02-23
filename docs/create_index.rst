CREATE INDEX
============

CREATE [UNIQUE] INDEX index_name [USING {BTREE | HASH}] ON table_name (column_name [length] [ASC | DESC], ...);

An example:

.. code-block:: mysql

   CREATE INDEX table_1_index ON table_1 (a,b);

This would create an index on table_t named  table_1_index that convered
columns a and b.

.. todo::
   
   how this is implemented currently. i.e. no fast add index
