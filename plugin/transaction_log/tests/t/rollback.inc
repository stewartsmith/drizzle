# 
# We start a transaction, roll it back, and test
# that the transaction has not made it to the log.
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

SET AUTOCOMMIT= 0;

CREATE TABLE t1 (
  id INT NOT NULL PRIMARY KEY
, padding VARCHAR(200) NOT NULL
);

# The above is committed already since CREATE
# TABLE implicitly does a commit.

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

# Try rolling back the above insertions...

ROLLBACK;

# Let's try an explicitly-started transaction as well

START TRANSACTION;

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

ROLLBACK;

START TRANSACTION;
INSERT INTO t1 VALUES (1, "I love testing.");
SAVEPOINT `savept1`;
INSERT INTO t1 VALUES (2, "I hate testing.");
ROLLBACK TO SAVEPOINT savept1;
COMMIT;

START TRANSACTION;
SAVEPOINT `savept2`;
INSERT INTO t1 VALUES (2, "I hate testing.");
RELEASE SAVEPOINT `savept2`;
SAVEPOINT `savept3`;
INSERT INTO t1 VALUES (3, "I love testing.");
SAVEPOINT `savept4`;
ROLLBACK TO SAVEPOINT savept3;
COMMIT;

DROP TABLE t1;

# The transaction log at this point should not
# contain anything other than the CREATE TABLE
# and the DROP TABLE.
