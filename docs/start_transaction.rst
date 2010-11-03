START TRANSACTION
======================

START TRANSACTION [WITH CONSISTENT SNAPSHOT]

Will begin a transaction. SET AUTOMCOMMIT must be set to zero in order for
this to work. The transaction is then run until either the connection to the
database is dropped, or a rollback or a commit command is sent.
