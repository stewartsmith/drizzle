Updating Data
=============

In Drizzle you can make use of UPDATE to modify an existing record in a table.

An example query:

.. code-block:: mysql

   UPDATE TABLE_1 SET a="1" WHERE <condition>;

Multi-table delete and multi-table update code was removed from Drizzle.

Multi-update/delete can be accomplished through subqueries. For example:

.. code-block:: mysql

	UPDATE tableX SET tableXfield = (SELECT MAX(tableY.tableYfield) FROM tableY WHERE tableX.tableXfield = tableY.tableYfield)

In other database frameworks, multi-update and multi-delete are used to change information in *one* table, but the rows to change are determined by using more than one table. In that case, subqueries work to address the issue of changing information in one table based on information in more than one table.
