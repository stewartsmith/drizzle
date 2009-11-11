#
# Tests a update with a sub query 
#

--disable_warnings
DROP TABLE IF EXISTS t1;
#DROP TABLE IF EXISTS t2;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL
, padding VARCHAR(200) NOT NULL
, PRIMARY KEY (id)
);

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

#CREATE TABLE t2 (
#  id INT NOT NULL
#, padding VARCHAR(200) NOT NULL
#, PRIMARY KEY (id)
#);

#INSERT INTO t2 VALUES (4, "I like to procrastinate");
#UPDATE t1 SET id=(SELECT MAX(id) FROM t2) WHERE padding='I hate testing.';
UPDATE t1 SET id=4 WHERE id=2;
DROP TABLE t1;
