COMMIT
======

COMMIT [WORK] [AND [NO] CHAIN] [[NO] RELEASE]

Calling COMMIT will cause the current transaction to save itself.

A COMMIT statement ends a transaction within Drizzle and makes all
changes visible to other users. The order of events is typically to
issue a START TRANSACTION statement, execute one or more SQL
statements, and then issue a COMMIT statement. Alternatively, a
ROLLBACK statement can be issued, which undoes all the work performed
since START TRANSACTION was issued. A COMMIT statement will also
release any existing savepoints that may be in use.

For example, DML statements do not implicitly commit the current
transaction. If a user's DML statements have been used to update some
data objects, and the updates need to be permanently recorded in the
database, you can use the COMMIT command.

An example:

.. code-block:: mysql

	START TRANSACTION;

	INSERT INTO popular_sites (url, id)
   		VALUES ('flickr.com', 07);

	INSERT INTO popular_sites (url, id)
   		VALUES ('twitter.com', 10);

	SELECT * FROM popular_sites;

+-----+---------------+-------+---------------------+
| id  | url           | notes | accessed            |
+=====+===============+=======+=====================+
| 07  | flickr.com    | NULL  | 2011-02-03 08:33:31 |
+-----+---------------+-------+---------------------+
| 10  | twitter.com   | NULL  | 2011-02-03 08:39:16 |
+-----+---------------+-------+---------------------+

Then to save the information just inserted, simply issue the COMMIT command:

.. code-block:: mysql

	COMMIT;
