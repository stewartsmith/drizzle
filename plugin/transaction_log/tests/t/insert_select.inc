# 
# Simple test of the serial event log for single INSERT SELECT statements
# 
# We create a table and insert some records
# into it.  We then create another table and
# INSERT SELECT from the first table into the 
# new one.
# 
# We then use the command_reader in drizzled/message/ to read the events.
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
);

INSERT INTO t2 SELECT * FROM t1;

DROP TABLE t1, t2;
