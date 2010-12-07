Dynamic SQL
===========

In Drizzle you can use the EXECUTE command along with :doc:'user defined variables <variables>'
to create SQL in a dynamic manner on the server. An exmaple of this is:

SET @var= "SELECT 1";
EXECUTE @var;

You can also omit the variable and just insert the SQL directly:

EXECUTE "SELECT 1";

By adding WITH NO RETURN you can have EXECUTE then no errors will be
generated and no data will be returned by the execution of the statement.

If you want to launch the query in a separate session, you can do that with
the following:
EXECUTE "SELECT 1" CONCURRENT;

The query will run in a new session and will execute as the user that
launched it. It can be killed via KILL and the system limit on total number
of sessions will be enforced.
