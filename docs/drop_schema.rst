DROP SCHEMA
============

DROP SCHEMA drops all tables and related objects in a schema and deletes it. It only does this once all queries or DML have ceased on them. At this point, it removes the schema itself from the catalog that owned it.

Syntax: ::

	DROP SCHEMA schema_name

Most storage engines do not have transactional DDL that is isolated from other transactions, so the incomplete effects of DROP SCHEMA will be viewable by other transactions.
