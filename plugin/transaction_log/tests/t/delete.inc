# 
# Simple test of the serial event log for single INSERT/DELETE statements
# 
# We create a table and insert some records
# into it then delete a record. 
# 
# We then use the transaction_reader in plugin/transaction_log/utilities to read the events.
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
INSERT INTO t1 VALUES (2, "I hate testing.");

DELETE FROM t1 where id = 1;

DROP TABLE t1;

# Test the situation where no keys (WHERE clause)
# are specified in a DELETE statement.  In the absence
# of triggers, this equates to a TRUNCATE TABLE statement, 
# and this is what should be written to the transaction log, 
# not multiple DeleteRecord events.
#
# However, right now this optimization does not occur. We
# write individual DeleteRecord message to the log.  We will
# optimize this away once TableShare has been refactored

CREATE TABLE t1 (
  id INT NOT NULL
, other INT NOT NULL
, PRIMARY KEY (id)
);

INSERT INTO t1 VALUES (1, 1);
INSERT INTO t1 VALUES (2, 2);
INSERT INTO t1 VALUES (3, 3);
INSERT INTO t1 VALUES (4, 4);
INSERT INTO t1 VALUES (5, 5);
INSERT INTO t1 VALUES (6, 6);
INSERT INTO t1 VALUES (7, 7);
INSERT INTO t1 VALUES (8, 8);

# This should produce a TRUNCATE event
DELETE FROM t1;

DROP TABLE t1;

# Test for LP Bug #496101:
#
# Delete within a transaction does not generate the correct 
# statements in the transaction log.  We start a transaction
# and issue both inserts and deletes in the same transaction.

--echo Start Test of LP Bug #496101

CREATE TABLE t1 (
  id INT NOT NULL
, padding VARCHAR(200) NOT NULL
, PRIMARY KEY (id)
);

START TRANSACTION;

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

DELETE FROM t1 where id = 1;

COMMIT;

DROP TABLE t1;

--echo End Test of LP Bug #496101
