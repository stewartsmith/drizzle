Using transactions
==================

.. toctree::
   :maxdepth: 2

   start_transaction
   commit
   rollback 
   savepoints

Drizzle is a transactional database by default and by design, meaning that changes and queries to the database appear to be Atomic, Consistent, Isolated, and Durable (ACID). This means that Drizzle implements `serializable <http://en.wikipedia.org/wiki/Serializability>`_, ACID transactions, even if the transaction is interrupted. Interruptions could include a program crash, an operating system failure, or loss of power to a computer. 

It collects operations that form tasks and stores them as a single operation, or if that operation is not possible it removes all changes attempted. Transactions are controlled via START TRANSACTION, ROLLBACK, and COMMIT. Savepoints are implemented to allow for a low leverl of granulairty.

A COMMIT statement ends a transaction within Drizzle and makes all changes visible to other users.  The order of events is typically to issue a START TRANSACTION statement, execute one or more SQL statements, and then issue a COMMIT statement. Alternatively, a ROLLBACK statement can be issued, which undoes all the work performed since START TRANSACTION was issued. A COMMIT statement will also release any existing savepoints that may be in use.

Drizzle can operate in an autocommit mode, where each statement is committed, via: ::

	SET AUTOCOMMIT= 1

Currently DDL operations are performed as a single transaction, this limitation will be lifted in the future.


