JOIN
====

'TODO'

.. note::
   Implicit cartesian products of the form ``SELECT * FROM t1, t2``
   without a ``WHERE`` or ``ON`` condition will error. If such
   behavior is intended please use ``SELECT * FROM t1 CROSS JOIN t2``.


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
