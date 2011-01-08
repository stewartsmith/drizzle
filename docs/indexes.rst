Indexes
=======

To quote wikipedia, "An index is a data structure that improves the
speed of data retrieval operations on a database table at the cost of slower
writes and increased storage space. Indexes can be created using one or more
columns of a database table, providing the basis for both rapid random
lookups and efficient access of ordered records. The disk space required to
store the index is typically less than that required by the table (since
indexes usually contain only the key-fields according to which the table is
to be arranged, and exclude all the other details in the table), yielding
the possibility to store indexes in memory for a table whose data is too
large to store in memory."


.. toctree::
   :maxdepth: 2

   create_index
   drop_index
