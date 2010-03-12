# 
# Simple test of the behaviour of the transaction log
# when temporary tables are involved in certain events.
#

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
--enable_warnings

CREATE TEMPORARY TABLE t1 (
  id INT NOT NULL PRIMARY KEY
, padding VARCHAR(200) NOT NULL
);

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

CREATE TABLE t2 LIKE t1;

INSERT INTO t2 SELECT * FROM t1;

DROP TABLE t1;
DROP TABLE t2;
