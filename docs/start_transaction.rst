START TRANSACTION
======================

A transaction can be started with either the BEGIN or START TRANSACTION statements. It can also be started by any statement when AUTOCOMMIT is disabled. A transaction can then run until either the connection to the database is dropped (in which case it is rolled back), or an explicit rollback or a commit command is sent.

.. code-block:: mysql

	START TRANSACTION [WITH CONSISTENT SNAPSHOT]
	BEGIN

.. warning::

   If you are currently already in a transaction, Drizzle will give a warning
   stating that you are in a transaction in a similar way to PostgreSQL.
   This is instead of implicitly committing the transaction in the way MySQL
   does.
