# 
# Test to verify updates from SELECT FOR UPDATE are handled 
# properly, an update is not put in the log for a update that
# times out. 
# 

SET @orig_lock_wait_timeout= @@innodb_lock_wait_timeout;
SELECT @orig_lock_wait_timeout;
SET GLOBAL innodb_lock_wait_timeout=2;

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
INSERT INTO t1 VALUES (5, "I still hate testing.");

connect (conn1, localhost, root,,);
connect (conn2, localhost, root,,);

connection conn1;
START TRANSACTION;
SELECT id FROM t1 FOR UPDATE;

connection conn2;
# This should timeout and not appear in the transaction log
--error ER_LOCK_WAIT_TIMEOUT
UPDATE t1 SET id=1000 WHERE padding='I love testing.';

connection conn1;
UPDATE t1 SET id=id + 1;
COMMIT;

connection default;
disconnect conn1;
disconnect conn2;
DROP TABLE t1;

SET GLOBAL innodb_lock_wait_timeout=@orig_lock_wait_timeout ;
