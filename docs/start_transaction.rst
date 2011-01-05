START TRANSACTION
======================

How do you begin a transaction in Drizzle? First of all, SET AUTOMCOMMIT must be set to zero. A transaction can then run until either the connection to the database is dropped, or a rollback or a commit command is sent. ::

	START TRANSACTION [WITH CONSISTENT SNAPSHOT]
