# 
# Test to verify updates from SELECT FOR UPDATE are handled 
# properly 
# 

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL
, padding VARCHAR(200) NOT NULL
, PRIMARY KEY (id)
);

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (3, "I hate testing.");
INSERT INTO t1 VALUES (5, "I hate testing.");

START TRANSACTION;
SELECT id FROM t1 FOR UPDATE;
UPDATE t1 SET id=id + 1;
COMMIT;

DROP TABLE t1;

