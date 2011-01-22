START TRANSACTION
======================

A transaction can be started with either the BEGIN or START TRANSACTION statements. It can also be started by any statement when AUTOCOMMIT is disabled. A transaction can then run until either the connection to the database is dropped (in which case it is rolled back), or an explicit rollback or a commit command is sent. ::

	START TRANSACTION [WITH CONSISTENT SNAPSHOT]
	BEGIN

When https://bugs.launchpad.net/drizzle/+bug/674719 is fixed, Drizzle will have the PostgreSQL like behaviour of giving an error if a transaction is already in progress.
