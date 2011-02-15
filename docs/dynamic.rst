Dynamic SQL
===========

Dynamic SQL enables you to build SQL statements dynamically at runtime. It can be generated within an application or from the system tables and then executed against the database to manipulate the data. You can create more general purpose, flexible applications by using dynamic SQL since the full text of a SQL statement may be unknown at the time of compilation.

Dynamic SQL allows you to execute DDL statements (and other SQL statements) that are not supported in purely static SQL programs.

In Drizzle you can use the EXECUTE command along with :doc:'user defined variables <variables>'
to create SQL in a dynamic manner on the server. An exmaple of this is: ::

	SET @var= "SELECT 1";
	EXECUTE @var;

You can also omit the variable and just insert the SQL directly: ::

	EXECUTE "SELECT 1";

By adding WITH NO RETURN you can have EXECUTE and no errors will be
generated and no data will be returned by the execution of the statement.

If you want to launch the query in a separate session, you can do that with
the following: ::

	EXECUTE "SELECT 1" CONCURRENT;

The query will run in a new session and will execute as the user that
launched it. It can be killed via KILL and the system limit on total number
of sessions will be enforced.

.. todo::

   EXECUTE executes the statements inside an explicit transaction.
