KILL
====

Calling KILL will either terminate a running query, or disconnect a connection, i.e. a session, from the database. ::

	KILL [CONNECTION | QUERY] session_id;

How it works: your connection sets a 'kill' flag in the session you want to kill. At various points in query execution, a session will check if the kill flag has been set, and if so, will quickly exit. (So while the KILL command doesn't technically kill the session immediately, it should appear to be fairly instantaneous.)

To kill the query being executed by a thread but leave the connection active, use the KILL QUERY command instead, followed by the appropriate thread ID.

Some engines such as Innobase will also check for the kill flag during a lock wait, allowing you to kill a session waiting for a row lock.
