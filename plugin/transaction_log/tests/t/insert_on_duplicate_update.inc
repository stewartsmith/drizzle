# 
# Simple test of the transaction log for INSERT ...
# ON DUPLICATE KEY UPDATE statements
# 
# We create a table and insert some records
# into it.  We then issue an INSERT ... ON DUPLICATE KEY UPDATE
# statement which will affect an existing record.
# 
# We then use the transaction_reader in plugin/transaction_log/utilities to read the events.
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY
, padding VARCHAR(200) NOT NULL
);

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

INSERT INTO t1 VALUES (2, "I love testing")
ON DUPLICATE KEY UPDATE padding="I love testing";

DROP TABLE t1;
