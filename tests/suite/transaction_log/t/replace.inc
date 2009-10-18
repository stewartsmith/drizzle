# 
# Simple test of the transaction log for testing REPLACE command 
# 
# We create a table then fill it with a few records and then
# issue a few REPLACE statements on it.
#

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL
, padding VARCHAR(200) NOT NULL
);

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

# Should delete original and insert a new one
# with a different "padding" column value...

REPLACE INTO t1 VALUE (2, "I love testing.");

DROP TABLE t1;
