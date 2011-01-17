String Data Types
=================

The string types in Drizzle are explained in the following groupings:

VARCHAR and VARBINARY
---------------------

A VARCHAR or VARBINARY type is used to store variable length data. Indexes on these types are by default the full length of the data stored. The only difference between the two types is the COLLATION which is used. VARBINARY uses a binary collation for all index usage.

TEXT and BLOB
-------------

A TEXT or BLOB type is used to store data which is over XXX in size. Indexes on these types must specify the number of character or bytes which should be used. The only difference between the two types is the COLLATION which is used. A BLOB uses a binary collation for all index usage.

ENUM
----

Enum (enumerated) types are static lists of strings that are defined on table creation. They can be used to represent a collection of string types that are sorted based on the order that they are created.

TODO - UTF-8. Multiple collations.
