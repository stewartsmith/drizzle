# 
# Tests that a value is put in the transaction log for rand()
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL
, PRIMARY KEY (id)
);

INSERT INTO t1 VALUES (1);
INSERT INTO t1 SELECT RAND(100)*100 FROM t1;

DROP TABLE t1;
