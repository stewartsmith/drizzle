Join
====

The JOIN keyword is used in an SQL statement to query data from two or more tables, based on a relationship between certain columns in these tables.

Queries can access multiple tables at once, or access the same table in such a way that multiple rows of the table are being processed at the same time. For instance, in order to list all the 'interest' records together with the location of the associated city. To do this, it's necessary to compare the city column of each row of the 'Interests" table with the name column of all rows in the cities table, and select the pairs of rows where these values match. As such, a JOIN statement involves combining records from two tables by using values common to each. 

Take the "Interests" table:

+---------+--------------+--------------+-------------+----------+
|Id       |DateAdded     |InterestType  |Name         | userID   |
+=========+==============+==============+=============+==========+
| 1       |2011-01-02    | Sport        |45           |2         |
+---------+--------------+--------------+-------------+----------+
| 2       |2011-01-02    | Art          |10           |4         |
+---------+--------------+--------------+-------------+----------+
| 3       |2011-01-02    | Music        |25           |1         |
+---------+--------------+--------------+-------------+----------+
| 4       |2011-01-02    | Food         |125          |1         |
+---------+--------------+--------------+-------------+----------+
| 5       |2011-01-03    | Music        |40           |2         |
+---------+--------------+--------------+-------------+----------+
| 6       |2011-01-03    | Food         |20           |3         |
+---------+--------------+--------------+-------------+----------+


Note that the "Id" column is the primary key in the "Persons" table. This means that no two rows can have the same "Id"; it distinguishes two interests even if they have the same name or userID.

Next, we have the "Persons" table:

+---------+------------+----------+----------+--------+
|userId	  |LastName    |FirstName |Address   |  City  |
+=========+============+==========+==========+========+
| 1 	  | Larson     | Sue      |3 Cherry  | Chicago|
+---------+------------+----------+----------+--------+
| 2 	  | Roberts    | Teri 	  |21 Brown  | Chicago|
+---------+------------+----------+----------+--------+
| 3 	  | Peterson   | Kari 	  |30 Mell   | Reno   |
+---------+------------+----------+----------+--------+
| 4	  | Anderson   | Kyle 	  |435 Tyler | Dayton |
+---------+------------+----------+----------+--------+

The "userID" column is the primary key in the "Persons" table; in the "Persons" table, it can be used to identify users without using their names. Therefore, the relationship between the two tables above is the "userId" column.

Different kinds of SQL JOINs
----------------------------

Here are the types of JOIN you can use, and the differences between them:

	**JOIN:** Return rows when there is at least one match in both tables

	**LEFT JOIN:** Return all rows from the left table, even if there are no matches in the right table

	**RIGHT JOIN:** Return all rows from the right table, even if there are no matches in the left table

	**CROSS JOIN:** Return rows when there is a match in one of the tables


.. note::
   Implicit cartesian products of the form ``SELECT * FROM t1, t2`` without a ``WHERE`` or ``ON`` condition will error. If such behavior is intended please use ``SELECT * FROM t1 CROSS JOIN t2``.


How joins are executed
----------------------

In its simplest form, a nested loop join works like this: It compares each row from one table (which can be considered the outer table) to each row from the other table (which can be considered the inner table), looking for rows that satisfy the join predicate. ('Inner table' and 'outer table' simply correlate to the inputs of the join, while 'inner join' and 'outer join' refer to the logical operations.)

The total number of rows compared is proportional to the size of the outer table multiplied by the size of the inner table. To minimize the cost of the operation, reduce or minimize the number of inner rows that we must compared to each outer row.

Nested loops support:

    * Inner join
    * Left outer join
    * Cross join
    * Cross apply and outer apply
    * Left semi-join and left anti-semi-join
