Executing Queries 
=================

Queries retrieve data from a database based on specific criteria. They
are performed with the declarative SELECT statement, which has no
persistent effects on the database. SELECT simply retrieves data from
one or more tables, or expressions.

A query includes a list of columns to be included in a result set; an example of this would be:  ::

	SELECT * FROM table_1;

SELECT * FROM is an example of using SELECT with a clause. Other keywords and clauses include:

The FROM clause
The WHERE clause
The GROUP BY clause
The HAVING clause
The ORDER BY clause