# 
# Simple test of the serial event log for multi-value INSERT statements
# 
# We create a table and insert some records
# into it.
# 
# We then use the transaction_reader in plugin/transaction_log/utilities to read the events.
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL PRIMARY KEY
, padding VARCHAR(200) NOT NULL
);

INSERT INTO t1 VALUES (1, "I love testing."), (2, "I hate testing.");

DROP TABLE t1;
