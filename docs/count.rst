COUNT
-----

Take the following "Nodes" table, where 'nodes' are user-contributed content:

+--------+-------------------+------------+----------------+-------------------+
|NodeID  |ContributionDate   |NodeSize    |NodePopularity  |UserName           |
+--------+-------------------+------------+----------------+-------------------+
|1	 |12/22/2005         |160	  |2	           |Smith              |
+--------+-------------------+------------+----------------+-------------------+
|2	 |08/10/2005	     |190	  |2	           |Johnson            |
+--------+-------------------+------------+----------------+-------------------+
|3  	 |07/13/2005	     |500	  |5	           |Baldwin            |
+--------+-------------------+------------+----------------+-------------------+
|4	 |07/15/2005	     |420	  |2               |Smith              |
+--------+-------------------+------------+----------------+-------------------+
|5	 |12/22/2005         |1000	  |4               |Wood               |
+--------+-------------------+------------+----------------+-------------------+
|6       |10/2/2005	     |820	  |4	           |Smith              |
+--------+-------------------+------------+----------------+-------------------+

The SQL COUNT function returns the number of rows in a table satisfying the criteria specified in the WHERE clause. If we want to count how many orders has made a customer with CustomerName of Smith, we will use the following SQL COUNT expression: ::

	SELECT COUNT (*) FROM Nodes
	WHERE UserName = 'Smith'

In the above statement, the COUNT keyword returns the number 3, because the user Smith has made 3 orders in total.

If you don’t specify a WHERE clause when using the COUNT keyword, your statement will simply return the total number of rows in the table, which would be 6 in this example: ::

	SELECT COUNT(*) FROM Nodes

How can we get the number of unique customers that have ordered from our store? We need to use the DISTINCT keyword along with the COUNT function to accomplish that: ::

	SELECT COUNT (DISTINCT UserName) FROM Nodes