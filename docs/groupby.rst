Group By
========

The GROUP BY clause is used to extract only those records that fulfill a specified criterion.

SQL GROUP BY Syntax ::

	SELECT column_name, aggregate_function(column_name)
	FROM table_name
	WHERE column_name operator value
	GROUP BY column_name

	
WHERE Clause Example

The "Activities" table:

+---------+--------------+-------------+----------+
|Id       |ActivityDate  |ActivityType |User      |
+=========+==============+=============+==========+
| 1       |              | Sue         |Larson    | 
+---------+--------------+-------------+----------+
| 2       | Roberts      | Teri        |Roberts   |
+---------+--------------+-------------+----------+
| 3       | Peterson     | Kari        |Peterson  | 
+---------+--------------+-------------+----------+
| 4       | Larson       | Sue         |Smith     | 
+---------+--------------+-------------+----------+
| 5       | Roberts      | Teri        |Dagwood   |
+---------+--------------+-------------+----------+
| 6       | Peterson     | Kari        |Masters   | 
+---------+--------------+-------------+----------+
 
If you want to select only the persons living in the city "Chicago" from the table above, use the following SELECT statement: ::

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