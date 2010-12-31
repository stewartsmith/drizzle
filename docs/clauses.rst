Using Clauses
=============

The WHERE Clause 

The WHERE clause is used to extract only those records that fulfill a specified criterion.

SQL WHERE Syntax ::

	SELECT column_name(s)
	FROM table_name
	WHERE column_name operator value
	
WHERE Clause Example

The "Persons" table:

+---------+------------+----------+----------+--------+
|Id 	  |LastName    |FirstName |Address   |  City  |
+=========+============+==========+==========+========+
| 1 	  | Larson     | Sue      |3 Cherry  | Chicago|
+---------+------------+----------+----------+--------+
| 2 	  | Robers     | Teri 	  |21 Brown  | Chicago|
+---------+------------+----------+----------+--------+
| 3 	  | Pettersen  | Kari 	  |30 Mell   | Reno   |
+---------+------------+----------+----------+--------+

Now we want to select only the persons living in the city "Sandnes" from the table above.

We use the following SELECT statement:
SELECT * FROM Persons
WHERE City='Chicago'

The result-set will look like this:

+---------+------------+----------+----------+--------+
| Id 	  |LastName    |FirstName |Address   |City    |
+---------+------------+----------+----------+--------+
|1 	  | Larson     | Sue 	  |3 Cherry  | Chicago|
+---------+------------+----------+----------+--------+
|2 	  | Roberts    | Teri 	  |21 Brown  | Chicago|
+---------+------------+----------+----------+--------+