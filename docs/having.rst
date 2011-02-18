Having
======

The WHERE keyword cannot be used with aggregate functions, but the HAVING clause can be; this is its primary use.

SQL HAVING Syntax:

.. code-block:: mysql

	SELECT column_name, aggregate_function(column_name)
	FROM table_name
	WHERE column_name operator value
	GROUP BY column_name
	HAVING aggregate_function(column_name) operator value

**SQL HAVING Example**

Take a look at the "Activities" table:

+---------+--------------+--------------+-------------+----------+
|Id       |ActivityDate  |ActivityType  |ActivityCost | userID   |
+=========+==============+==============+=============+==========+
| 1       |2011-01-02    | Sport        |45           |131       |
+---------+--------------+--------------+-------------+----------+
| 2       |2011-01-02    | Art          |10           |256       |
+---------+--------------+--------------+-------------+----------+
| 3       |2011-01-02    | Music        |25           |022       |
+---------+--------------+--------------+-------------+----------+
| 4       |2011-01-02    | Food         |125          |022       |
+---------+--------------+--------------+-------------+----------+
| 5       |2011-01-03    | Music        |40           |131       |
+---------+--------------+--------------+-------------+----------+
| 6       |2011-01-03    | Food         |20           |175       |
+---------+--------------+--------------+-------------+----------+

In order to find if any users have spent more than $100 on recreational activities, use the following SQL statement:

.. code-block:: mysql

	SELECT userID,SUM(ActivityCost) FROM Activities
	GROUP BY userID
	HAVING SUM(ActivityCost)>100;

The result-set will look like this:

======    ===============
userID    SUM(OrderPrice)
======    ===============
022       150
======    ===============

Now we want to find if userIDs "131", "256", or "175" spent less than $50 on Activities.

We add an ordinary WHERE clause to the SQL statement:

.. code-block:: mysql

	SELECT userID,SUM(ActivityCost) FROM Activities
	WHERE userID='131' OR userID='256' OR userID="175"
	GROUP BY userID
	HAVING SUM(ActivityCost)<50;

The result-set would be:

======    ===============
userID    SUM(OrderPrice)
======    ===============
256       10
175       20
======    ===============
