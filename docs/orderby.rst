ORDER BY
========

You can use `ORDER BY` to specify the order of the rows returned for a query.
Without an `ORDER BY` clause, rows may be returned in any order. Some storage
engines are more deterministic about the order they return rows than others,
so while it may appear that you always get rows back in the same order without
an `ORDER BY` clause, this is a coincidence.

SQL ORDER BY Syntax:

.. code-block:: mysql

	SELECT column_name(s)
	FROM table_name
	ORDER BY column_name(s) ASC|DESC;

You can provide a list of columns and if results should be in ascending or descending order.
