# 
# Simple test of the serial event log for testing ALTER command 
# 
# We create a table then alter it by adding a column into it. 
# 
# We then use the transaction_reader in plugin/transaction_log/utilities to read the events.
#

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL
, padding VARCHAR(200) NOT NULL
, PRIMARY KEY (id)
);

RENAME TABLE t1 TO t2;

DROP TABLE t2;
