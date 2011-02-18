Where
=====

The WHERE clause is used to extract only those records that fulfill a specified criterion.

Simple SQL WHERE Syntax:

.. code-block:: mysql

	SELECT column_name(s)
	FROM table_name
	WHERE column_name operator value
	
**Simple WHERE Clause Example**

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
 
If you want to select only the persons living in the city "Chicago" from the table above, use the following SELECT statement:

.. code-block:: mysql

	SELECT * FROM Persons
	WHERE City='Chicago'

The result-set will look like this:

+---------+------------+----------+----------+--------+
| Id 	  |LastName    |FirstName |Address   |City    |
+=========+============+==========+==========+========+
|1 	  | Larson     | Sue 	  |3 Cherry  | Chicago|
+---------+------------+----------+----------+--------+
|2 	  | Roberts    | Teri 	  |21 Brown  | Chicago|
+---------+------------+----------+----------+--------+
