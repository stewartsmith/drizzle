# 
# Test that tables with multi-column primary keys
# are handled properly in replication
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (
  k1 INT NOT NULL
, k2 INT NOT NULL
, padding VARCHAR(200) NOT NULL
, PRIMARY KEY (k1, k2)
);

INSERT INTO t1 VALUES (1, 1, "I love testing.");
INSERT INTO t1 VALUES (2, 2, "I hate testing.");
INSERT INTO t1 VALUES (2, 3, "I hate and love testing.");
INSERT INTO t1 VALUES (3, 3, "I adore testing.");

# Simple PK update on both columns
UPDATE t1 SET padding= "XXX" WHERE k1= 1 AND k2= 1;

# UPDATE all records in table matching first column
# in primary key
UPDATE t1 SET padding= "YYY" WHERE k1= 2;

# UPDATE all records in table matching second column
# in primary key
UPDATE t1 SET padding= "ZZZ" WHERE k2= 3;

# UPDATE all records in table
UPDATE t1 SET padding= "AAA";

DROP TABLE t1;
