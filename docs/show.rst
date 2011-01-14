SHOW
=====

All SHOW commands are shorthand forms of queries on the INFORMATION_SCHEMA
and/or DATA_DICTIONARY. More data can be
gained by exeucting queries directly on the tables found either the
DATA_DICTIONARY or the INFORMATION_SCHEMA.

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

Shows the CREATE TABLE statement used to create the table table_name.

SHOW PROCESSLIST
----------------

