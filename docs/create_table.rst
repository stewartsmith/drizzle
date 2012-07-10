CREATE TABLE
============

'TODO'

.. code-block:: mysql

    CREATE [TEMPORARY] TABLE [IF NOT EXISTS] table_name
      (create_definition, ...)
      [engine_options]
      REPLICATE=[TRUE|FALSE] 

or:

.. code-block:: mysql

    CREATE [TEMPORARY] TABLE [IF NOT EXISTS] table_name
      [(create_definition, ...)]
      [engine_options]
      select_statement
      REPLICATE=[TRUE|FALSE] 

or:

.. code-block:: mysql

    CREATE [TEMPORARY] TABLE [IF NOT EXISTS] table_name
      LIKE different_table_name
      [engine_options]
      REPLICATE=[TRUE|FALSE] 

create_definition
-----------------

::

    column_name column_definition
    [CONSTRAINT [symbol] ] PRIMARY KEY [index_type]
    (index_column_name, ...)
    INDEX [index_name] (index_column_name, ...)
    (index_column_name, ...)
    [CONSTRAINT [symbol] ] UNIQUE [INDEX]
    (index_column_name, ...)
    [CONSTRAINT [symbol] ] FOREIGN KEY [index_name] (index_column_name, ...)
    reference_definition
    CHECK (expr)

column_definition
-----------------

::

	data_type [NOT NULL | NULL] [DEFAULT default_value]
    [AUTO_INCREMENT] [UNIQUE [KEY] | [PRIMARY] KEY]
    [COMMENT 'string']
    [reference_definition]

data_type
---------

	* INTEGER
	* BIGINT
	* DOUBLE[(length, decimals)]
	* DECIMAL[(length[,decimals])]
	* DATE
	* TIMESTAMP
	* DATETIME
	* VARCHAR(length) [COLLATE collation_name]
	* VARBINARY(length)
	* BLOB
	* TEXT [BINARY] [COLLATE collation_name]
	* ENUM(value1, value2, value3, ...) [COLLATE collation_name]

reference_option
----------------

  RESTRICT | CASCADE | SET NULL | NO ACTION

engine_options
---------------

    engine_option [[,] engine_option] ...

engine_option
-------------

  ENGINE = engine_name
  { engine_specific }

REPLICATE
---------

Specify whether or not a TABLE should be replicated.

  REPLICATE=[TRUE|FALSE] 
