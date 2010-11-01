Obtaining Information on the Contents of the Database
=====================================================

Drizzle provides two schema per catalog that contain data about the contents
of the schema, the state of the database instance, and other additional
information that might be useful to a user. In addition Drizzle supports
"SHOW" commands, and DESCRIBE, which can be used by a user to gain
additional information.

The INFORMATION_SCHEMA only contains data that is the SQL standard requires.
More information can be gained by the tables found in the DATA_DICTIONARY
schema.

.. toctree::
   :maxdepth: 2

   data_dictionary
   information_schema
   show
   describe

