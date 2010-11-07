# 
# Simple test of the transaction log for statements involving
# BLOB fields.
#

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL
, padding BLOB NOT NULL
, PRIMARY KEY (id)
);

INSERT INTO t1 VALUES (1, 'test\0me');
INSERT INTO t1 VALUES (2, 'test\0you');

UPDATE t1 SET padding = 'test\0you' WHERE padding = 'test\0me';

DELETE FROM t1 WHERE padding = 'test\0you';

CREATE TABLE t2 (
  id INT NOT NULL
, padding BLOB
, PRIMARY KEY (id)
);

INSERT INTO t2 VALUES (1, NULL);
INSERT INTO t2 VALUES (2, 'test\0you');
UPDATE t2 SET padding = NULL WHERE padding = 'test\0you'; 
DELETE FROM t2 WHERE padding IS NULL;

DROP TABLE t1, t2;
