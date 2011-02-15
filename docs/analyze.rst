ANALYZE
=======

ANALYZE TABLE table_name [, table_name] ...

ANALYZE TABLE read locks a table, and then analyzes and stores the key distribution for a table.

.. todo::

   is read lock always true?

.. todo::
   
   some engines don't perform an explicit gathering of statistics when
   you type ANALYZE. e.g. innobase (which only copies it's current estimate).
   Only recently did I add this to HailDB so that it does go and do the index
   dives on ANALYZE.
