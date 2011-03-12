Distinct
========

In a table, columns may contain more than one of the same value. 

Sometimes it's helpful to list only the different, distinct values in a table; in this case the DISTINCT keyword can be used.

SQL SELECT DISTINCT Syntax: 

.. code-block:: mysql

	SELECT DISTINCT column_name(s)
	FROM table_name

**SELECT DISTINCT Example**

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

In order to select distinct values from the column named "City" from the table above, use the following SELECT statement:

.. code-block:: mysql

	SELECT DISTINCT City FROM Persons;

The result-set will look like this:

+--------+
|City    |
+========+
|Chicago |
+--------+
|Reno    |
+--------+
