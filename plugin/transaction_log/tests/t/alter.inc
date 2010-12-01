# 
# Simple test of the transaction log for testing ALTER command 
#

--disable_query_log
#SELECT MAX(TRANSACTION_ID) FROM DATA_DICTIONARY.INNODB_REPLICATION_LOG INTO @min_transaction_id;
--enable_query_log

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

# Simple addition of a new column
ALTER TABLE t1 ADD dummy INT;

# Add an index via ALTER TABLE
ALTER TABLE t1 ADD INDEX ix_padding (padding(20));

--disable_query_log
#eval SELECT MAX(id) FROM t1 INTO @id;
--enable_query_log
#eval SELECT TRANSACTION_MESSAGE_STRING FROM DATA_DICTIONARY.INNODB_REPLICATION_LOG WHERE TRANSACTION_ID= @min_transaction_id;

DROP TABLE t1;

