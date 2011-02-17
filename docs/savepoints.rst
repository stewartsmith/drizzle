SAVEPOINT
=========

A savepoint is a marker inside a transaction that allows all commands that are executed after it was established to be rolled back. It effectively restores the transaction state to what it was at the time of the savepoint. ::

	SAVEPOINT identifier

This sets a savepoint that can be returned to in the current transaction. The "identifier" is the name given to the new savepoint. If the identifier has already been used then the original identifier is replaced. Example:

.. code-block:: mysql

     SAVEPOINT A;
     INSERT INTO t1 values (1);
     SAVEPOINT A;
     INSERT INTO t1 values (2);
     ROLLBACK TO SAVEPOINT A;

Will only roll back the second insert statement.
