DROP SCHEMA
============

DROP SCHEMA removes all tables and related objects from a schema once all queries or DML have ceased
on them. It then removes the schema itself from the catalog that owned it.

DROP SCHEMA schema_name

Most storage engines do not have transactional DDL that is isolated from
other transactions, so the incomplete effects of DROP SCHEMA will be viewable by
other transactions.
