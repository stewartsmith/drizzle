#
# Test that create with select works 
#

--disable_warnings
DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS t2;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL PRIMARY KEY
, padding VARCHAR(200) NOT NULL
);

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

CREATE TABLE t2 (
  id INT NOT NULL PRIMARY KEY
, padding VARCHAR(200) NOT NULL
) SELECT * FROM t1;

DROP TABLE t1, t2;
