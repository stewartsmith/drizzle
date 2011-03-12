FLUSH
=====

The FLUSH statement clears or reloads different internal caches. One variant acquires a lock.

.. code-block:: mysql

   FLUSH flush_option [, flush_option] ...

flush_option
------------

* TABLES table_name [, table_name]
	
(closes all specified tables, forces those tables in use to be closed, and flushes the query cache for the named tables)

* TABLES WITH READ LOCK
	
(closes all open tables and locks all tables for all databases with a global read lock*)

* LOGS
	
(closes and reopens all log files--if binary logging is enabled, the sequence number of the binary log file is incremented by one relative to the previous file)

* STATUS
	
(adds the current thread's session status variable values to the global values and resets the session values to zero)
    
To release a FLUSH TABLES WITH READ LOCK, you must issue an UNLOCK TABLES.
