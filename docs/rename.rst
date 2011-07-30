RENAME
======

Rename a table, or group of tables.

If you have an existing table old_name, you can create another table new_name; it will be empty but reptant he same structure, and then replace the existing table with the empty one as follows (assuming backup_table does not already exist):

.. code-block:: mysql

	CREATE TABLE new_name (...);
	RENAME TABLE old_name TO backup_table, new_name TO old_name;

When using a statement to rename more than one table, the order of operations are done from left to right. To swap two table names, use the following (assuming tmp_table does not already exist):

.. code-block:: mysql

	RENAME TABLE old_name TO tmp_table,
        new_name TO old_name,
        tmp_table TO new_name;

While RENAME is running, no other session can access any of the involved tables. 

.. seealso::
   :doc:`/alter_table`
