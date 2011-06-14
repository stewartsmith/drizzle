TRUNCATE
========

This will delete all data in a table but unlike a DROP TABLE it will allow you to keep the table in your database.

.. code-block:: mysql

	TRUNCATE TABLE table_name

TRUNCATE TABLE is typically faster than a DELETE * FROM TABLE query. An
unbounded DELETE query will have to generate log data for every
row in the table, which could be quite large. TRUNCATE TABLE is the same
as DROP followed by CREATE except that the absence of the table between
DROP and CREATE is not exposed.