Order By
========

The ORDER BY keyword is used to sort the result-set by column; by default, it sorts the records in ascending order.

SQL ORDER BY Syntax:

.. code-block:: mysql

	SELECT column_name(s)
	FROM table_name
	ORDER BY column_name(s) ASC|DESC;

**ORDER BY Example**

The "Persons" table:

+---------+------------+----------+----------+--------+
|Id 	  |LastName    |FirstName |Address   |  City  |
+=========+============+==========+==========+========+
| 1 	  | Larson     | Sue      |3 Cherry  | Chicago|
+---------+------------+----------+----------+--------+
| 2 	  | Roberts    | Teri 	  |21 Brown  | Chicago|
+---------+------------+----------+----------+--------+
| 3 	  | Peterson   | Kari 	  |30 Mell   | Reno   |
+---------+------------+----------+----------+--------+

To select all the persons from the table above, and also sort them by their last name, use the following SELECT statement:

.. code-block:: mysql

	SELECT * FROM Persons
	ORDER BY LastName;

The result-set will look like this:

+---------+------------+----------+----------+--------+
|Id 	  |LastName    |FirstName |Address   |  City  |
+=========+============+==========+==========+========+
| 1 	  | Larson     | Sue      |3 Cherry  | Chicago|
+---------+------------+----------+----------+--------+
| 3 	  | Peterson   | Kari 	  |30 Mell   | Reno   |
+---------+------------+----------+----------+--------+
| 2 	  | Roberts    | Teri 	  |21 Brown  | Chicago|
+---------+------------+----------+----------+--------+

Without using "ORDERBY" in the following query, the result-set will be non-deterministic, and could returned matching rows in a different order for each query. 

ORDER BY DESC can be used to reverse the order of the result set.

.. code-block:: mysql

	SELECT * FROM Persons
	ORDER BY LastName DESC;


.. todo::

   add something about how ORDER BY is executed. index scan vs filesort
