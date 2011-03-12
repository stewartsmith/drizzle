Replacing Data
==============

In Dizzle you can make use of REPLACE to insert a new record into a table or
delete and insert a record in a table if a record matching the primary key
already exists. If no primary key or unique constraint is found on the table
then REPLACE is equivalent to INSERT.

A typical query:

.. code-block:: mysql

   REPLACE INTO table_1 SET a=5;

or

.. code-block:: mysql

   REPLACE INTO table_1 VALUES (4);


REPLACE is an extension to the SQL Standard that first appeared in MySQL.
