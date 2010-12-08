TRUNCATE
========

This will delete all data in a table but unlike a DROP TABLE it will allow you to keep the table in your database. It deletes the rows but leaves all counters, such as a SERIAL, in place. ::

	TRUNCATE TABLE table_name
