# 
# Simple test of the transaction log and the errors
# which should be produced when a table is inserted
# into when the transaction log is enabled but the
# table does not have primary keys.
# 
# We create a table without a primary key and insert 
# a record into it.  An error should be produced.
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL
, padding VARCHAR(200) NOT NULL
);

# The below should produce an error because the CREATE TABLE
# above does not have a primary key.
--error ER_NO_PRIMARY_KEY_ON_REPLICATED_TABLE
INSERT INTO t1 VALUES (1, "I love testing.");

DROP TABLE t1;
