Obtaining Information on the Database Contents
==============================================

Drizzle provides two schema per catalog that provide information about the contents of the database, the state of the database instance, and additional user-centric information. In addition, Drizzle supports SHOW and DESCRIBE commands, which can be used to obtain additional information.

The INFORMATION_SCHEMA only contains data that the SQL standard requires. More information can be gained by the tables found in the DATA_DICTIONARY schema.

.. toctree::
   :maxdepth: 2

   data_dictionary
   information_schema
   show
   describe

