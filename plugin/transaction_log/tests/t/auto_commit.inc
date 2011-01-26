# 
# Simple test of the serial event log for autocommit behaviour.
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

SET AUTOCOMMIT= 0;

CREATE TABLE t1 (
  id INT NOT NULL PRIMARY KEY
, padding VARCHAR(200) NOT NULL
);

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

COMMIT;
DROP TABLE t1;

# Try the same thing with auto_commit turned on
# This should produce a commit between inserts...

SET AUTOCOMMIT= 1;

CREATE TABLE t1 (
  id INT NOT NULL PRIMARY KEY
, padding VARCHAR(200) NOT NULL
);

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

DROP TABLE t1;
