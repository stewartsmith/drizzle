KILL
====

KILL [CONNECTION | QUERY] session_id

Calling KILL will terminate a either a running query, or disconnect a
connection, i.e. a session, from the database.

The way this works is that your connection sets a 'kill' flag in the session
you want to kill. At various points in query execution, a session will
check if the kill flag has been set, and if so, will quickly exit. So while
the KILL command will not strictly immediately kill the session, it should
appear to be fairly instantaneous.

Some engines such as Innobase will also check for the kill flag
during a lock wait, allowing you to kill a session waiting for a row lock.
