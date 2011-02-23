TRUNCATE
========

.. todo::

   I don't think the below SERIAL/AUTO_INCREMENT thing below is true.

This will delete all data in a table but unlike a DROP TABLE it will allow you to keep the table in your database. It deletes the rows but leaves all counters, such as a SERIAL, in place.

.. code-block:: mysql

	TRUNCATE TABLE table_name

TRUNCATE TABLE is typically faster than a DELETE * FROM TABLE query. An
unbounded DELETE query will have to generate undo log data for every
row in the table, which could be quite large. TRUNCATE TABLE is the same
as DROP followed by CREATE except that the absence of the table between
DROP and CREATE is not exposed.

