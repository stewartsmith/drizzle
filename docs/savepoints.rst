SAVEPOINT
=========

'TODO' ::

	SAVEPOINT identifier

'TODO'

.. code-block:: mysql

     SAVEPOINT A;
     INSERT INTO t1 values (1);
     SAVEPOINT A;
     INSERT INTO t1 values (2);
     ROLLBACK TO SAVEPOINT A;

'TODO'
