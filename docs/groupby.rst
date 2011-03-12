Group By
========

The GROUP BY clause is used to extract only those records that fulfill a specified criterion.

SQL GROUP BY Syntax

.. code-block:: mysql

	SELECT column_name, aggregate_function(column_name)
	FROM table_name
	WHERE column_name operator value
	GROUP BY column_name

**GROUP BY Clause Example**

The "Activities" table:

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

Running the following simple query

.. code-block:: mysql

	SELECT userID
	FROM activities
	GROUP BY userID;

Returns:

+---------+
| userID  |
+=========+
| 131     |
+---------+
| 256     |
+---------+
| 022     |
+---------+
| 175     |
+---------+

(This shows that GROUP BY accepts a column_name and consolidates like customer values.)

However, GROUP BY is much more powerful when used with an aggregate function. Let's say you want to find the total amount spent by each unique User.

You could use the following SQL statement:

.. code-block:: mysql

	SELECT userID,SUM(ActivityCost) AS "Activity Total"
	FROM Activities
	GROUP BY userID;

The result-set will look like this:

======    ==============
userID    Activity Total
======    ==============
131       85
256       10
022       150
175       20
======    ==============

With the aggregate SUM() function, SQL can calculate how much each unique user has spent on activities over time.

We can also use the GROUP BY statement on more than one column, like this:

.. code-block:: mysql

	SELECT userID,ActivityDate,SUM(ActivityCost)
	FROM Activities
	GROUP BY userID,ActivityDate;

