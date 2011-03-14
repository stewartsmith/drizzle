ANALYZE
=======

Syntax:

.. code-block:: mysql

	ANALYZE [LOCAL | NO_WRITE_TO_BINLOG] TABLE tbl_name [, tbl_name] 

ANALYZE TABLE usually read locks a table, and then analyzes and stores
the key distribution for a table.

ANALYZE functionality differs depending on the storage engine.

On InnoDB tables, using ANALYZE will result in a WRITE LOCK on the
table. It also will not perform an explicit gathering of statistics
when you issue an ANALYZE command. To update the index cardinality,
there will be 10 random dives into each index, retrieving an estimated
cardinality. Therefore, several ANALYZE TABLEs in a row are likely to
produce different results each time.

Statistics-gathering with ANALYZE has recently been added to HailDB so
that index dives are performed.
