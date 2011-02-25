Using transactions
==================

.. toctree::
   :maxdepth: 2

   start_transaction
   commit
   rollback
   savepoints

The essence of a transaction is that it groups multiple steps into a single, all-or-nothing operation. Drizzle is a transactional database by default and by design, meaning that changes and queries to the database appear to be Atomic, Consistent, Isolated, and Durable (ACID). This means that Drizzle implements `serializable <http://en.wikipedia.org/wiki/Serializability>`_, ACID transactions, even if the transaction is interrupted.

NOTE: Drizzle still supports non-transactional storage engines, and if these are used then you will not get transactional behaviour with them. The default engine is transactional.

Transactions are a group of operations that form tasks and stores them as a single operation, or if that operation is not possible it removes all changes attempted. Transactions are controlled via START TRANSACTION, ROLLBACK, and COMMIT. Savepoints are implemented to allow for a lower level of granularity.

A COMMIT statement ends a transaction within Drizzle and makes all changes visible to other users.  The order of events is typically to issue a START TRANSACTION statement, execute one or more SQL statements, and then issue a COMMIT statement. Alternatively, a ROLLBACK statement can be issued, which undoes all the work performed since START TRANSACTION was issued. A COMMIT statement will also release any existing savepoints that may be in use.

Drizzle can operate in an autocommit mode, where each statement is committed at the end of statement, via:

.. code-block:: mysql

	SET AUTOCOMMIT= 1

If you set AUTOCOMMIT=1 during a transaction, that transaction will be committed as part of the SET AUTOCOMMIT=1 statement.

Transactional DDL is currently not supported, although it may be in the future. This means that although currently you will get a ER_TRANSACTION_DDL_NOT_SUPPORTED error message if you try and execute DDL statements within a transaction, in future versions of Drizzle they may succeed.

Currently DDL operations are performed as a single transaction, but this limitation will be lifted in the future.

For various reasons, Drizzle may have to ROLLBACK a statement or transaction
without having been asked to do so. Examples include lock wait timeout or
deadlock detection. Any application using a transactional database system
needs to be able to deal with such cases.
