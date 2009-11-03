# 
# Simple test of the serial event log for single INSERT/DELETE statements
# 
# We create a table and insert some records
# into it then delete a record. 
# 
# We then use the transaction_reader in drizzled/message/ to read the events.
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL
, padding VARCHAR(200) NOT NULL
, PRIMARY KEY (id)
);

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

DELETE FROM t1 where id = 1;
