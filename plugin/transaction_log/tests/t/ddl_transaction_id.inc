# 
# Tests ordering of transaction ID, that SELECT queries
# do not increment the transaction ID, and that the proper
# generation of transaction IDs is done for DDL operations.
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

SELECT * FROM t1;

ALTER TABLE t1 CHANGE COLUMN padding less_padding VARCHAR(180) NOT NULL;

# 
# Should be no result here...
#

SELECT * FROM t1 WHERE id = 3;

DROP TABLE t1;
