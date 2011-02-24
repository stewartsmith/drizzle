HailDB
======

HailDB is a Storage Engine plugin that uses the HailDB library
(`http://www.haildb.com <http://www.haildb.com>`_) for storage. The HailDB
library is based off the innodb_plugin. By having HailDB as a separate shared
library, you are able to upgrade HailDB with new features or bug fixes
without having to update your whole database server.

HailDB is intended to replace the inbuilt innobase plugin.

Current Advantages
------------------
 * Crash proof DDL
   Table definitions are stored inside the HailDB data files and modified
   inside the same transaction as the DDL operation. You can never get
   out of sync between tables in HailDB and the table definition.
 * Simpler engine code
   The HailDB storage engine code is much smaller and cleaner than the
   innobase plugin
 * Smaller and faster auto_increment implementation
   Without the legacy of auto_increment locking for MySQL replication,
   we are able to use a simple global atomic variable for each table.
 * Direct access to the HailDB DATA_DICTIONARY
   You can directly query the underlying (internal) data dictionary

Current Limitations
-------------------
 * Does not yet support FOREIGN KEYs
 * No semi-consistent read
 * No descending indexes
 * Some DATA_DICTIONARY views have not yet been ported over.
 * Tables without an explicit PRIMARY KEY get a hidden 64bit auto_increment
   primary key instead of the internal ROW_ID as a primary key.

Isolation Levels
----------------

HailDB supports: REPEATABLE READ, READ COMMITTED, SERIALIZABLE and READ
UNCOMMITTED isolation levels.

Row formats
-----------

HailDB can store the rows for a table in one of a few ways. This can be specified as an option to CREATE TABLE (example below).

.. code-block:: mysql

  CREATE TABLE t1 (
  	 pk bigint auto_increment primary key,
	 b blob
  ) ROW_FORMAT='COMPRESSED' ENGINE=InnoDB;

REDUNDANT
  Oldest row format. It was the default prior to MySQL 5.0.3. You probably
  no longer want to use this row format.

COMPACT
  Default since MySQL 5.0.3. It uses less space than the REDUNDANT row format
  for variable length filends and nulls.

DYNAMIC
  (Requires Barracuda file format or higher). TEXT and BLOB fields are stored
  separately to the rest of the row.

COMPRESSED
  (Requires Barracuda file format or higher). Similar to Dynamic, but database
  pages are compressed. This trades off slightly increased CPU usage for smaller
  on disk tables.
