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
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY
, padding VARCHAR(200) NOT NULL
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

# This will actually execute an UPDATE for InnoDB, 
# as this is an optimized scenario that can have the
# REPLACE INTO converted into an INSERT ... ON DUPLICATE
# KEY UPDATE.

REPLACE INTO t1 VALUE (2, "I love testing.");

DROP TABLE t1;

CREATE TABLE t1 (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY
, padding VARCHAR(200) NOT NULL
) ENGINE=InnoDB;
CREATE TABLE t2 (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY
, fk_id INT NOT NULL
, CONSTRAINT fk_t1 FOREIGN KEY (fk_id) REFERENCES t1 (id) ON DELETE CASCADE
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

# Should delete original and insert a new one
# with a different "padding" column value...

REPLACE INTO t1 VALUE (2, "I love testing.");

DROP TABLE t2, t1;
