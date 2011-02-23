SHOW
=====

All SHOW commands are shorthand forms of queries on the INFORMATION_SCHEMA and/or DATA_DICTIONARY. More data can be gained by executing queries directly on the tables found either the DATA_DICTIONARY or the INFORMATION_SCHEMA.

SHOW SCHEMAS
------------

Will list the names of all the schemas in the current catalog.

SHOW TABLES
-----------

Will list the names of all the tables in the current schema.

SHOW TEMPORARY TABLES
---------------------

SHOW TABLE STATUS
-----------------

Will show the current status of tables for the current database which are
currently in the table cache.  A query (such as SELECT) may be needed to add a
table to the table cache so that it can be shown in this output.

SHOW COLUMNS FROM table_name
----------------------------

SHOW INDEXES from table_name
----------------------------


SHOW WARNINGS
-------------

Shows the warnings and/or errors from the previous command.

SHOW ERRORS
-----------

Shows errors from the previous command.

SHOW CREATE SCHEMA schema_name
------------------------------

Shows the CREATE SCHEMA command required to recreate schema_name.

SHOW CREATE TABLE table_name
----------------------------

Shows the CREATE TABLE statement used to create the table table_name.  Please
note the AUTO_INCREMENT in this is the AUTO_INCREMENT specified at CREATE or
ALTER TABLE time, not the current AUTO_INCREMENT value.

SHOW PROCESSLIST
----------------

