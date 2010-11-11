Dynamic SQL
===========

In Drizzle you can use the EXECUTE command along with user defined variables
to create SQL in a dynamic manner on the server. An exmaple of this is:

SET @var= "SELECT 1";
EXECUTE @var;

You can also omit the variable and just insert the SQL directly:

EXECUTE "SELECT 1";

By adding WITH NO RETURN you can have EXECUTE then no errors will be
generated and no data will be returned by the execution of the statement.
