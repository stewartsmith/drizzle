Executing Queries
=================

Queries retrieve data from a database based on specific criteria. They
are performed with the declarative SELECT statement, which has no
persistent effects on the database. SELECT simply retrieves data from
one or more tables, or expressions.

A query includes a list of columns to be included in a result set; an example of this would be:

.. code-block:: mysql

	SELECT * FROM table_name;

SELECT * FROM is an example of using SELECT with clauses. The select clause specifies the columns you want to retrieve. The from clause specifies the tables to search. 

Keywords and clauses include:

.. toctree::
   :maxdepth: 2

   where
   distinct
   groupby
   having
   orderby
   join

For example:

.. code-block:: mysql

	SELECT first_column_name, second_column_name
	FROM table_name
	WHERE first_column_name > 1000;

The column names that follow the SELECT keyword determine the columns that will be returned in the results. You can select as many column names that you'd like, or you can use a * to select all columns (as shown in the first example above). The order in which they are specified will be reflected in your query results.

The table name that follows the keyword FROM specifies the table that will be queried to retrieve the desired results.

The WHERE clause (optional) specifies the data values or rows to be returned or displayed, based on the criteria described after the keyword WHERE.
