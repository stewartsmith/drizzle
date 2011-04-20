Dynamic SQL
===========

Dynamic SQL enables you to build SQL statements dynamically at runtime. It can be generated within an application or from the system tables and then executed against the database to manipulate the data. You can create more general purpose, flexible applications by using dynamic SQL since the full text of a SQL statement may be unknown at the time of compilation.

Dynamic SQL allows you to execute DDL statements (and other SQL statements) that are not supported in purely static SQL programs.

EXECUTE
--------

EXECUTE() is used to generate SQL in a dynamic manner on the server. It can select values inside of the database as variables, and then execute them from statements that are already stored as variables.

EXECUTE can be CONCURRENT and with NO RETURN.

In Drizzle you can use the EXECUTE command along with :doc:'user defined variables <variables>'
to create SQL in a dynamic manner on the server. An example of this is:

.. code-block:: mysql

	SET @var= "SELECT 1";
	EXECUTE @var;

You can also omit the variable and just insert the SQL directly:

.. code-block:: mysql

	EXECUTE "SELECT 1";

By adding WITH NO RETURN you can have EXECUTE and no errors will be
generated and no data will be returned by the execution of the statement.

If you want to launch the query in a separate session, you can do that with
the following:

.. code-block:: mysql

	EXECUTE "SELECT 1" CONCURRENT;

The query will run in a new session and will execute as the user that
launched it. It can be killed via KILL and the system limit on total number
of sessions will be enforced.

EXECUTE also executes the statements inside an explicit transaction.

The EXECUTE class has a run() method, which takes a std::string with SQL (to execute). Here is a simple example from the slave plugin code:

.. code-block:: mysql

	Execute execute(*(_session.get()), true);
	string sql("SELECT `transaction_id` FROM `sys-replication`.`queue`"
               " WHERE `commit_order` IS NOT NULL ORDER BY `commit_order` ASC");
	sql::ResultSet result_set(1);
	execute.run(sql, result_set);

In essence, EXECUTE wraps SQL (single or multiple statements) in a transaction and executes it.
