Inserting Data
==============

In Drizzle you can make use of INSERT in order to insert data into a table.

A type query:

.. code-block:: mysql

   INSERT INTO A VALUES ("1");

INSERT statements that use VALUES syntax can insert multiple rows. To do this, use the multi row VALUES syntax (include multiple lists of column values, each enclosed within parentheses and separated by commas):

.. code-block:: mysql

	INSERT INTO music (artist, album, date_prod, genre) VALUES
    	('Beatles', 'Abbey Road', '1969-09-26', 'rock'),
   	('The Velvet Underground', 'The Velvet Underground', '1969-03-05', 'rock');

or:

.. code-block:: mysql
	
	INSERT INTO table_1 (a,b,c) VALUES(1,2,3),(4,5,6),(7,8,9);

The following statement is incorrect since the number of values in the list does not match the number of column names:

.. code-block:: mysql

	INSERT INTO table_1 (a,b,c) VALUES(1,2,3,4,5,6,7,8,9);

VALUE is a synonym for VALUES where performing a single or multirow INSERT.

Performance
-----------

A multi-row INSERT involving three rows will require roughly one third of the time required to execute the three single-row statements. This performance improvement can become quite significant over a large number of statements. 
