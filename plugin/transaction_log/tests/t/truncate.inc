# 
# Simple test of the serial event log for testing TRUNCATE command 
# 
# We create a table and insert some data then call truncate on the 
# table.
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

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

TRUNCATE TABLE t1;

DROP TABLE t1;
