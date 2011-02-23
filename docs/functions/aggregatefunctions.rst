Aggregate Functions
===================

SQL group (aggregate) functions operate on sets of values. If you use an aggregate function in a statement containing no GROUP BY clause, it is equivalent to grouping on all rows.

General syntax for aggregate functions is:

.. code-block:: mysql

	SELECT "function type" ("column_name")
	FROM "table_name";

The following are examples of aggregate functions:

:ref:`avg`:  Return the average value of the argument. (Does not work with temporal values unless first converted to numeric values.)

:ref:`count`
(DISTINCT):  Return the count of a number of different values

:ref:`count`:  Return a count of the number of rows returned
	
:ref:`group_concat`:  Return a concatenated string

:ref:`max`:  Return the maximum or minim values


.. _avg:

AVG
---

The AVG function returns the average value for the specified column in a table. To find the average session time for users and GROUP BY last_name:

.. code-block:: mysql

	SELECT last_name, AVG(session_length)
    	-> FROM session_details GROUP BY last_name;

AVG() returns NULL if there are no matching rows.  


.. _count:

COUNT
-----

Take the following "Nodes" table, where 'nodes' are user-contributed content:

+--------+-------------------+------------+----------------+-------------------+
|NodeID  |ContributionDate   |NodeSize    |NodePopularity  |UserName           |
+========+===================+============+================+===================+
|1	 |12/22/2010         |160	  |2	           |Smith              |
+--------+-------------------+------------+----------------+-------------------+
|2	 |08/10/2010	     |190	  |2	           |Johnson            |
+--------+-------------------+------------+----------------+-------------------+
|3  	 |07/13/2010	     |500	  |5	           |Baldwin            |
+--------+-------------------+------------+----------------+-------------------+
|4	 |07/15/2010         |420	  |2               |Smith              |
+--------+-------------------+------------+----------------+-------------------+
|5	 |12/22/2010         |1000	  |4               |Wood               |
+--------+-------------------+------------+----------------+-------------------+
|6       |10/2/2010          |820	  |4	           |Smith              |
+--------+-------------------+------------+----------------+-------------------+

The SQL COUNT function returns the number of rows in a table satisfying the criteria specified in the WHERE clause. If we want to count the number of nodes made by user Smith, we will use the following SQL COUNT expression:

.. code-block:: mysql

	SELECT COUNT * FROM Nodes
	WHERE UserName = "Smith";

In the above statement, the COUNT keyword returns the number 3, because the user Smith has 3 total nodes.

If you don't specify a WHERE clause when using the COUNT keyword, your statement will simply return the total number of rows in the table, which would be 6 in this example:

.. code-block:: mysql

	SELECT COUNT * FROM Nodes;


.. _group_concat:

GROUP CONCAT
-------------

GROUP_CONCAT returns a string result with the concatenated non-NULL values from a group.

For example, without GROUP_CONCAT, this query:

.. code-block:: mysql

	SELECT id,client_id FROM services WHERE id = 3;

Returns:

+----+-----------+
| id | client_id |
+====+===========+
|  3 |         7 |
+----+-----------+
|  3 |         8 |
+----+-----------+
|  3 |         9 |
+----+-----------+

But using GROUP_CONCAT in an alternate query:

.. code-block:: mysql

	SELECT id,GROUP_CONCAT(client_id) FROM services WHERE id = 3 GROUP BY id;

Will return:

+----+-------------------------+
| id | GROUP_CONCAT(client_id) |
+====+=========================+
|  3 | 7,8,9                   |
+----+-------------------------+


.. _max:

MAX and MIN
------------

MAX returns the maximum value in a group. In cases where MAX is passed a string argument, it will return the maximum string value.

MIN returns the minimum value of a group. Like MAX, MIN returns the minimum string value string value. 

MAX and MIN return NULL if there are no matching rows.

.. code-block:: mysql

	SELECT product_id, MIN(price), MAX(price)
		FROM inventory
		GROUP BY product_id;

** Question: For MAX(), does Drizzle compare ENUM and SET columns by their string value rather than by the string's relative position in the set? **
