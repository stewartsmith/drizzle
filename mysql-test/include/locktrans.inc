# include/locktrans.inc
#
# Transactional LOCK TABLE tests

# eval SET SESSION STORAGE_ENGINE = $engine_type;

--disable_warnings
DROP DATABASE  IF EXISTS mysqltest;
DROP TABLE     IF EXISTS t1, t2, t3, t4, v1;
DROP VIEW      IF EXISTS t1, t2, t3, t4, v1;
DROP PROCEDURE IF EXISTS lock_t1_excl;
DROP PROCEDURE IF EXISTS count_t2;
DROP PROCEDURE IF EXISTS update_t2;
DROP TRIGGER   IF EXISTS t1_ai;
--enable_warnings

--echo #
--echo # WL3561 - transactional LOCK TABLE - Syntax tests
--echo # ================================================
#
eval CREATE TABLE t1 (c1 INT ) ENGINE=$engine_type;
eval CREATE TABLE t2 (c2 INT ) ENGINE=$engine_type;
eval CREATE TABLE t3 (c3 INT ) ENGINE=$engine_type;
#
--echo #
--echo # Valid syntax for non-transactional locks.
LOCK TABLE t1 READ, t2 WRITE;
UNLOCK TABLES;
LOCK TABLE t1 READ LOCAL, t2 LOW_PRIORITY WRITE;
UNLOCK TABLES;
#
--echo #
--echo # Valid syntax for transactional locks.
LOCK TABLE t1 IN SHARE MODE, t2 IN EXCLUSIVE MODE;
UNLOCK TABLES;
#
if ($nowait_support)
{
--echo #
--echo # Valid syntax for transactional locks with NOWAIT option.
--echo ## In the preliminary reference implementation we expect these errors:
--echo ## NOWAIT+SHARE="timed out", NOWAIT+EXCLUSIVE="not supported".
--echo ## Statements abort on first error.
--error ER_LOCK_WAIT_TIMEOUT
LOCK TABLE t1 IN SHARE MODE NOWAIT, t2 IN EXCLUSIVE MODE NOWAIT;
--error ER_UNSUPPORTED_EXTENSION
LOCK TABLE t1 IN EXCLUSIVE MODE NOWAIT, t2 IN SHARE MODE NOWAIT;
--error ER_LOCK_WAIT_TIMEOUT
LOCK TABLE t1 IN EXCLUSIVE MODE, t2 IN SHARE MODE NOWAIT;
}
#
--echo #
--echo # Valid syntax for aliases with and without 'AS'.
LOCK TABLE t1 AS a1 READ, t2 a2 WRITE;
UNLOCK TABLES;
LOCK TABLE t1 AS a1 IN SHARE MODE, t2 a2 IN EXCLUSIVE MODE;
UNLOCK TABLES;
#
--echo #
--echo # Transactional locks taken on a view.
CREATE VIEW v1 AS SELECT * FROM t1, t2 WHERE t1.c1 = t2.c2;
LOCK TABLE v1 IN SHARE MODE;
LOCK TABLE v1 IN EXCLUSIVE MODE;
DROP VIEW v1;
#
--echo #
--echo # Locking INFORMATION_SCHEMA fails on missing privileges.
--error ER_DBACCESS_DENIED_ERROR
LOCK TABLE information_schema.tables IN SHARE MODE;
--error ER_DBACCESS_DENIED_ERROR
LOCK TABLE information_schema.tables IN EXCLUSIVE MODE;
--error ER_DBACCESS_DENIED_ERROR
LOCK TABLE information_schema.tables READ;
--error ER_DBACCESS_DENIED_ERROR
LOCK TABLE information_schema.tables WRITE;
#
--echo #
--echo # The new keywords EXCLUSIVE and NOWAIT are not reserved words.
eval CREATE TABLE t4 (exclusive INT, nowait INT) ENGINE=$engine_type;
LOCK TABLE t4 WRITE;
DROP TABLE t4;
#
--echo #
--echo # Syntax errors for misspelled modes or left out symbols.
--echo ##-------------------------------------------------------
--error ER_PARSE_ERROR
LOCK TABLE t1 IN SHARED MODE;
--error ER_PARSE_ERROR
LOCK TABLE t1 SHARE MODE;
--error ER_PARSE_ERROR
LOCK TABLE t1 IN SHARE;
--error ER_PARSE_ERROR
LOCK TABLE t1 IN MODE;
--error ER_PARSE_ERROR
LOCK TABLE t1 READ NOWAIT, t2 WRITE NOWAIT;
--error ER_PARSE_ERROR
LOCK TABLE t1 READ NOWAIT, t2 IN EXCLUSIVE MODE NOWAIT;
--error ER_PARSE_ERROR
LOCK TABLE t1 IN SHARE MODE NOWAIT, t2 WRITE NOWAIT;
--error ER_PARSE_ERROR
LOCK TABLE t1 IN SHARED MODE NOWAIT;
--error ER_PARSE_ERROR
LOCK TABLE t1 SHARE MODE NOWAIT;
--error ER_PARSE_ERROR
LOCK TABLE t1 IN SHARE NOWAIT;
--error ER_PARSE_ERROR
LOCK TABLE t1 IN MODE NOWAIT;
--echo ##----------------------
--echo ## End of syntax errors.
#
--echo #
--echo #
--echo # WL3561 - transactional LOCK TABLE - Lock method conversion
--echo # ==========================================================
--echo #
--echo # Implicit lock method conversion due to mix in statement.
LOCK TABLE t1 READ, t2 IN EXCLUSIVE MODE;
UNLOCK TABLES;
--echo # Lock t1 share (converted to read), t2 write.
LOCK TABLE t1 IN SHARE MODE, t2 WRITE;
--echo # Show t1 is read locked, t2 write locked.
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
INSERT INTO t1 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t1;
#
--echo #
--echo # Implicit lock method conversion due to existing non-transact. locks.
--echo # Implicitly unlock existing non-transactional locks and take new ones.
--echo # Lock t1 exclusive (converted to write), t2 share (converted to read).
LOCK TABLE t1 IN EXCLUSIVE MODE, t2 IN SHARE MODE;
--echo # Show t1 is write locked, t2 read locked.
INSERT INTO t1 SELECT * FROM t2;
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
INSERT INTO t2 SELECT * FROM t1;
UNLOCK TABLES;
#
--echo #
--echo # Reject lock method conversion in strict mode.
--echo # Set strict mode.
SET @wl3561_save_sql_mode= @@SQL_MODE;
SET @@SQL_MODE= 'STRICT_ALL_TABLES';
--echo # Try mixed mode locks.
--error ER_NO_AUTO_CONVERT_LOCK_STRICT
LOCK TABLE t1 READ, t2 IN EXCLUSIVE MODE;
--error ER_NO_AUTO_CONVERT_LOCK_STRICT
LOCK TABLE t1 IN SHARE MODE, t2 WRITE;
--echo # Lock non-transactional.
LOCK TABLE t1 READ, t2 WRITE;
--echo # Try transactional locks on top of the existing non-transactional locks.
--error ER_NO_AUTO_CONVERT_LOCK_STRICT
LOCK TABLE t1 IN SHARE MODE, t2 IN EXCLUSIVE MODE;
--echo ## Error is reported on first table only. Show both errors:
SHOW WARNINGS;
UNLOCK TABLES;
SET @@SQL_MODE= @wl3561_save_sql_mode;
#
--echo #
--echo # Reject lock method conversion in an active transaction.
--echo # Start transaction.
START TRANSACTION;
--echo # Try mixed mode locks.
--error ER_NO_AUTO_CONVERT_LOCK_TRANSACTION
LOCK TABLE t1 READ, t2 IN EXCLUSIVE MODE;
--error ER_NO_AUTO_CONVERT_LOCK_TRANSACTION
LOCK TABLE t1 IN SHARE MODE, t2 WRITE;
COMMIT;
#
--echo #
--echo # Implicit lock method conversion for non-transactional storage engine.
--echo # Create a non-transactional table.
eval CREATE TABLE t4 (c4 INT) ENGINE= $other_non_trans_engine_type;
--echo # Request a transactional lock, which is converted to non-transactional.
LOCK TABLE t4 IN SHARE MODE;
--echo # Try a conflict with the existing non-transactional lock.
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
INSERT INTO t4 VALUES(444);
UNLOCK TABLES;
--echo # Set strict mode.
SET @@SQL_MODE= 'STRICT_ALL_TABLES';
--echo # Try a transactional lock, which would need a conversion.
--error ER_NO_AUTO_CONVERT_LOCK_STRICT
LOCK TABLE t4 IN SHARE MODE;
SET @@SQL_MODE= @wl3561_save_sql_mode;
#
--echo #
--echo # View with transactional and non-transactional storage engine.
CREATE VIEW v1 AS SELECT * FROM t3, t4 WHERE t3.c3 = t4.c4;
--echo # Request a share lock on the view, which is converted to read locks.
LOCK TABLE v1 IN SHARE MODE;
--echo # Show that read locks on the base tables prohibit writing ...
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
INSERT INTO t3 SELECT * FROM t4;
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
INSERT INTO t4 SELECT * FROM t3;
--echo # ... but allow reading.
SELECT COUNT(*) FROM t3, t4 WHERE t3.c3 = t4.c4;
SELECT COUNT(*) FROM v1;
--echo ## Report conversion on view due to existing non-transactional locks.
LOCK TABLE v1 IN EXCLUSIVE MODE;
INSERT INTO t3 VALUES(333);
INSERT INTO t4 VALUES(444);
--error ER_TABLE_NOT_LOCKED
INSERT INTO t1 VALUES(111);
UNLOCK TABLES;
--echo ## Now report conversion on base table again.
LOCK TABLE v1 IN EXCLUSIVE MODE;
INSERT INTO t3 VALUES(333);
INSERT INTO t4 VALUES(444);
--error ER_TABLE_NOT_LOCKED
INSERT INTO t1 VALUES(111);
UNLOCK TABLES;
DROP VIEW v1;
TRUNCATE t4;
#
--echo #
--echo # Insufficient privileges do not unlock tables nor end transactions.
--echo # Prepare database, tables and an user with insufficient privileges.
--echo # Make a new connection with this user.
CREATE DATABASE mysqltest;
eval CREATE TABLE mysqltest.t5 (c5 INT) ENGINE=$engine_type;
eval CREATE TABLE mysqltest.t6 (c6 INT) ENGINE=$engine_type;
CREATE USER mysqltest_1@localhost;
GRANT SELECT, INSERT ON mysqltest.* TO mysqltest_1@localhost;
connect (conn1,localhost,mysqltest_1,,);
    --echo # connection conn1.
    connection conn1;
    --echo # Show sufficient privileges to lock tables in the test database.
    LOCK TABLE t1 READ, t2 WRITE;
    --echo # Show insufficient privileges in the mysqltest database.
    --error ER_DBACCESS_DENIED_ERROR
    LOCK TABLE mysqltest.t5 READ, mysqltest.t6 WRITE;
    --echo # Show that the locks in 'test' still exist.
    --error ER_TABLE_NOT_LOCKED_FOR_WRITE
    INSERT INTO t1 SELECT * FROM t2;
    INSERT INTO t2 SELECT * FROM t1;
    --echo # Unlock tables.
    UNLOCK TABLES;
    --echo # Start transaction.
    START TRANSACTION;
    --echo # Insert a value.
    INSERT INTO t1 VALUES(111);
    --echo # Try a lock that fails on privileges.
    --error ER_DBACCESS_DENIED_ERROR
    LOCK TABLE mysqltest.t5 READ;
    --echo # Rollback transaction.
    ROLLBACK;
    --echo # Show that the inserted value has gone.
    SELECT * FROM t1;
    --echo # Drop the connection with the unprivileged user.
    disconnect conn1;
--echo # connection default.
connection default;
--echo #
--echo # Sufficient privileges do unlock tables and end transactions.
--echo # Grant sufficient privileges to the user.
--echo # Make a new connection with this user.
GRANT SELECT, INSERT, LOCK TABLES ON mysqltest.* TO mysqltest_1@localhost;
connect (conn1,localhost,mysqltest_1,,);
    --echo # connection conn1.
    connection conn1;
    --echo # Lock tables in the test database.
    LOCK TABLE t1 READ, t2 WRITE;
    --echo # Lock tables in the mysqltest database.
    LOCK TABLE mysqltest.t5 READ, mysqltest.t6 WRITE;
    --echo # Show that the locks in 'test' have been replaced ...
    --error ER_TABLE_NOT_LOCKED
    INSERT INTO t1 SELECT * FROM t2;
    --error ER_TABLE_NOT_LOCKED
    INSERT INTO t2 SELECT * FROM t1;
    --echo # ... by the locks in 'mysqltest'.
    --error ER_TABLE_NOT_LOCKED_FOR_WRITE
    INSERT INTO mysqltest.t5 SELECT * FROM mysqltest.t6;
    INSERT INTO mysqltest.t6 SELECT * FROM mysqltest.t5;
    --echo # Unlock tables.
    UNLOCK TABLES;
    --echo # Start transaction.
    START TRANSACTION;
    --echo # Insert a value.
    INSERT INTO t1 VALUES(111);
    --echo # Take a new lock.
    LOCK TABLE mysqltest.t5 READ;
    --echo # Rollback transaction.
    ROLLBACK;
    UNLOCK TABLES;
    --echo # Show that the inserted value had been committed.
    SELECT * FROM t1;
    TRUNCATE t1;
    disconnect conn1;
--echo # connection default.
connection default;
#
UNLOCK TABLES;
DROP USER mysqltest_1@localhost;
DROP DATABASE mysqltest;
DROP TABLE t1, t2, t3, t4;
#
# Overcome timing problems DROP t1 -> CREATE t1 (*locktrans_myisam).
--sleep 0.1
#
--echo #
--echo # WL3594 - transactional LOCK TABLE Testing - Functional tests
--echo # ============================================================
#
--echo # Prepare tables and connections.
--echo # Set AUTOCOMMIT= 0 in each connection.
SET AUTOCOMMIT= 0;
eval CREATE TABLE t1 (c1 INT) ENGINE=$engine_type;
eval CREATE TABLE t2 (c2 INT) ENGINE=$engine_type;
connect (conn1,localhost,root,,);
    --echo # connection conn1.
    connection conn1;
    SET AUTOCOMMIT= 0;
connect (conn2,localhost,root,,);
        --echo # connection conn2.
        connection conn2;
        SET AUTOCOMMIT= 0;
--echo # connection default.
connection default;
#

if ($transactional)
{
--echo #
--echo # Transactional lock behaviour:
--echo # LOCK TABLE does _not_ commit a transaction.
--echo # Insert a value.
INSERT INTO t1 VALUES (111);
--echo # Lock transactional.
LOCK TABLE t1 IN EXCLUSIVE MODE;
--echo # Rollback.
ROLLBACK;
--echo # Show that the inserted value has gone.
SELECT * FROM t1;
--echo #
--echo # After LOCK TABLE one can access tables not mentioned in LOCK TABLE.
--echo # Lock t1 transactional.
LOCK TABLE t1 IN EXCLUSIVE MODE;
--echo # Insert a value into t2.
INSERT INTO t2 VALUES (222);
--echo # Show that the inserted value is indeed in the table.
SELECT * FROM t2;
--echo #
--echo # One can issue LOCK TABLE many times, adding more tables.
--echo # Lock t2 transactional.
LOCK TABLE t2 IN EXCLUSIVE MODE;
--echo #
--echo # LOCK TABLE does not rollback a transaction.
--echo # Show that the inserted value is still in the table.
SELECT * FROM t2;
--echo # Rollback transaction.
ROLLBACK;
--echo # Show that the inserted value has gone.
SELECT * FROM t2;
--echo #
--echo # Tables are unlocked at the end of transaction (commit).
--echo # Take an exclusive lock.
LOCK TABLE t1 IN EXCLUSIVE MODE;
--echo # Commit.
COMMIT;
    --echo # connection conn1.
    connection conn1;
    --echo # Take an exclusive lock.
    --echo # This would fail after timeout if t1 is still locked.
    LOCK TABLE t1 IN EXCLUSIVE MODE;
    --echo # Commit.
    COMMIT;
--echo # connection default.
connection default;
--echo #
--echo # Tables are unlocked at the end of transaction (rollback).
--echo # Take an exclusive lock.
LOCK TABLE t1 IN EXCLUSIVE MODE;
--echo # Rollback.
ROLLBACK;
    --echo # connection conn1.
    connection conn1;
    --echo # Take an exclusive lock.
    --echo # This would fail after timeout if t1 is still locked.
    LOCK TABLE t1 IN EXCLUSIVE MODE;
    --echo # Rollback.
    ROLLBACK;
--echo # connection default.
connection default;
--echo #
--echo # UNLOCK TABLES does not touch a transaction when
--echo # no non-transactional table locks exist.
--echo # Take a transactional lock.
LOCK TABLE t1 IN EXCLUSIVE MODE;
--echo # Insert a value.
INSERT INTO t1 VALUES(111);
--echo # Unlock (non-transactional) table locks.
UNLOCK TABLES;
--echo # Show that the inserted value is still in the table.
SELECT * FROM t1;
--echo # Rollback.
ROLLBACK;
--echo # Show that the inserted value has gone.
SELECT * FROM t1;
--echo #
--echo # UNLOCK TABLES commits a transaction when
--echo # non-transactional table locks exist.
--echo # Take a non-transactional lock.
LOCK TABLE t1 WRITE;
--echo # Insert a value.
INSERT INTO t1 VALUES(111);
--echo # Unlock (non-transactional) table locks.
UNLOCK TABLES;
--echo # Show that the inserted value is still in the table.
SELECT * FROM t1;
--echo # Rollback.
ROLLBACK;
--echo # Show that the inserted value is still in the table.
SELECT * FROM t1;
TRUNCATE t1;
--echo #
--echo # START TRANSACTION removes a previous lock.
--echo # Take an exclusive lock.
LOCK TABLE t1 IN EXCLUSIVE MODE;
--echo # Start transaction.
START TRANSACTION;
    --echo # connection conn1.
    connection conn1;
    --echo # Take an exclusive lock.
    --echo # This would fail after timeout if t1 is still locked.
    LOCK TABLE t1 IN EXCLUSIVE MODE;
    COMMIT;
--echo # connection default.
connection default;
COMMIT;
--echo #
--echo # With Auto commit on, transactional locks will be ignored
--echo # Set AUTOCOMMIT= 1.
SET AUTOCOMMIT= 1;
--echo # Take an exclusive lock.
LOCK TABLE t1 IN EXCLUSIVE MODE;
    --echo # connection conn1.
    connection conn1;
    --echo # Set AUTOCOMMIT= 1.
    SET AUTOCOMMIT= 1;
    --echo # Take an exclusive lock.
    --echo # This would fail after timeout if t1 is still locked.
    LOCK TABLE t1 IN SHARE MODE;
    SET AUTOCOMMIT= 0;
    COMMIT;
--echo # connection default.
connection default;
UNLOCK TABLES;
SET AUTOCOMMIT= 0;
COMMIT;
--echo #
--echo # With Auto commit on, transactional locks can time out.
--echo # Default connection runs in transactional mode.
--echo # Set AUTOCOMMIT= 0.
SET AUTOCOMMIT= 0;
--echo # Take an exclusive lock, which persists.
LOCK TABLE t1 IN EXCLUSIVE MODE;
    --echo # connection conn1.
    connection conn1;
    --echo # conn1 runs in autocommit mode.
    --echo # Set AUTOCOMMIT= 1.
    SET AUTOCOMMIT= 1;
    --echo # Try an exclusive lock,
    --echo # which times out though running in autocommit mode.
    --error ER_LOCK_WAIT_TIMEOUT
    LOCK TABLE t1 IN SHARE MODE;
    SET AUTOCOMMIT= 0;
    COMMIT;
--echo # connection default.
connection default;
UNLOCK TABLES;
SET AUTOCOMMIT= 0;
COMMIT;
#
} 

--echo #
--echo # Normal WRITE locks go before readers (autocommit).
--echo # Set AUTOCOMMIT= 1.
SET AUTOCOMMIT= 1;
--echo # Insert a value.
INSERT INTO t1 VALUES(111);
--echo # Take a non-transactional lock.
LOCK TABLE t1 READ;
    --echo # connection conn1.
    connection conn1;
    --echo # Set AUTOCOMMIT= 1.
    SET AUTOCOMMIT= 1;
    --echo # Take a non-transactional WRITE lock,
    --echo # which waits in background until first read lock is released.
    send LOCK TABLE t1 WRITE;
--echo # connection default.
connection default;
--echo # Wait for the helper thread to sit on its lock.
while (`SELECT COUNT(*) < 1 FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE STATE LIKE '%lock%'`)
{
  --sleep 0.1
}
        --echo # connection conn2.
        connection conn2;
        --echo # Set AUTOCOMMIT= 1.
        SET AUTOCOMMIT= 1;
        --echo # Take a non-transactional READ lock,
        --echo # which waits in background until the WRITE lock is released.
        send LOCK TABLE t1 READ;
--echo # connection default.
connection default;
--echo # Wait for the helper threads to sit on their locks.
while (`SELECT COUNT(*) < 2 FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE STATE LIKE '%lock%'`)
{
  --sleep 0.1
}
--echo # Unlock this connections non-transactional lock.
UNLOCK TABLES;
    --echo # connection conn1.
    connection conn1;
    --echo # Now the WRITE lock is taken.
    reap;
    --echo # Insert a value.
    INSERT INTO t1 VALUES(1111);
    --echo # Unlock table.
    UNLOCK TABLES;
        --echo # connection conn2.
        connection conn2;
        --echo # Now the READ lock is taken.
        reap;
        --echo # Select from the table.
        SELECT * FROM t1;
        --echo # Unlock table.
        UNLOCK TABLES;
--echo # connection default.
connection default;
TRUNCATE t1;
--echo #
--echo # LOW_PRIORITY WRITE locks wait for readers (autocommit).
--echo # Insert a value.
INSERT INTO t1 VALUES(111);
--echo # Take a non-transactional lock.
LOCK TABLE t1 READ;
    --echo # connection conn1.
    connection conn1;
    --echo # Take a non-transactional LOW_PRIORITY WRITE lock,
    --echo # which waits in background until all read locks are released.
    send LOCK TABLE t1 LOW_PRIORITY WRITE;
--echo # connection default.
connection default;
--echo # Wait for the helper thread to sit on its lock.
while (`SELECT COUNT(*) < 1 FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE STATE LIKE '%lock%'`)
{
  --sleep 0.1
}
        --echo # connection conn2.
        connection conn2;
        --echo # Take a non-transactional READ lock,
        --echo # which goes before the LOW_PRIORITY WRITE lock.
        LOCK TABLE t1 READ;
        --echo # The READ lock could be taken immediately.
        --echo # Select from the table.
        SELECT * FROM t1;
        --echo # Unlock table.
        UNLOCK TABLES;
        SET AUTOCOMMIT= 0;
--echo # connection default.
connection default;
--echo # Unlock this connections non-transactional lock.
UNLOCK TABLES;
    --echo # connection conn1.
    connection conn1;
    --echo # Now the LOW_PRIORITY WRITE lock is taken.
    reap;
    --echo # Insert a value.
    INSERT INTO t1 VALUES(1111);
    --echo # Unlock table.
    UNLOCK TABLES;
    SET AUTOCOMMIT= 0;
--echo # connection default.
connection default;
TRUNCATE t1;
SET AUTOCOMMIT= 0;
COMMIT;

if ($transactional) 
{
--echo #
--echo # Normal WRITE locks go before readers (transaction).
--echo # Insert a value.
INSERT INTO t1 VALUES(111);
COMMIT;
--echo # Take a non-transactional lock.
LOCK TABLE t1 READ;
    --echo # connection conn1.
    connection conn1;
    --echo # Take a non-transactional WRITE lock,
    --echo # which waits in background until first read lock is released.
    send LOCK TABLE t1 WRITE;
--echo # connection default.
connection default;
--echo # Wait for the helper thread to sit on its lock.
while (`SELECT COUNT(*) < 1 FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE STATE LIKE '%lock%'`)
{
  --sleep 0.1
}
        --echo # connection conn2.
        connection conn2;
        --echo # Take a non-transactional READ lock,
        --echo # which waits in background until the WRITE lock is released.
        send LOCK TABLE t1 READ;
--echo # connection default.
connection default;
--echo # Wait for the helper threads to sit on their locks.
while (`SELECT COUNT(*) < 2 FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE STATE LIKE '%lock%'`)
{
  --sleep 0.1
}
--echo # Unlock this connections non-transactional lock.
UNLOCK TABLES;
    --echo # connection conn1.
    connection conn1;
    --echo # Now the WRITE lock is taken.
    reap;
    --echo # Insert a value.
    INSERT INTO t1 VALUES(1111);
    --echo # Unlock table.
    UNLOCK TABLES;
        --echo # connection conn2.
        connection conn2;
        --echo # Now the READ lock is taken.
        reap;
        --echo # Select from the table.
        SELECT * FROM t1;
        --echo # Unlock table.
        UNLOCK TABLES;
--echo # connection default.
connection default;
TRUNCATE t1;
COMMIT;
--echo #
--echo # LOW_PRIORITY WRITE behaves like WRITE in transaction mode.
--echo # Insert a value.
INSERT INTO t1 VALUES(111);
COMMIT;
--echo # Take a non-transactional lock.
LOCK TABLE t1 READ;
    --echo # connection conn1.
    connection conn1;
    --echo # Take a non-transactional LOW_PRIORITY WRITE lock,
    --echo # which waits in background until first read lock is released.
    send LOCK TABLE t1 LOW_PRIORITY WRITE;
--echo # connection default.
connection default;
--echo # Wait for the helper thread to sit on its lock.
while (`SELECT COUNT(*) < 1 FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE STATE LIKE '%lock%'`)
{
  --sleep 0.1
}
        --echo # connection conn2.
        connection conn2;
        --echo # Take a non-transactional READ lock,
        --echo # which waits in background for the LOW_PRIORITY WRITE lock.
        send LOCK TABLE t1 READ;
--echo # connection default.
connection default;
--echo # Wait for the helper threads to sit on their locks.
while (`SELECT COUNT(*) < 2 FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE STATE LIKE '%lock%'`)
{
  --sleep 0.1
}
--echo # Unlock this connections non-transactional lock.
UNLOCK TABLES;
    --echo # connection conn1.
    connection conn1;
    --echo # Now the LOW_PRIORITY WRITE lock is taken.
    reap;
    --echo # Insert a value.
    INSERT INTO t1 VALUES(1111);
    --echo # Unlock table.
    UNLOCK TABLES;
        --echo # connection conn2.
        connection conn2;
        --echo # Now the READ lock is taken.
        reap;
        --echo # Select from the table.
        SELECT * FROM t1;
        --echo # Unlock table.
        UNLOCK TABLES;
--echo # connection default.
connection default;
TRUNCATE t1;
COMMIT;
#
}

if ($nowait_support)
{
--echo #
--echo # NOWAIT.
--echo # Take an exclusive lock.
LOCK TABLE t1 IN EXCLUSIVE MODE;
    --echo # connection conn1.
    connection conn1;
    --echo # Try an exclusive lock,
    --echo # which conflicts and cannot immediately be taken.
    --error ER_LOCK_WAIT_TIMEOUT
    LOCK TABLE t1 IN SHARE MODE NOWAIT;
--echo # connection default.
connection default;
--echo # Commit.
COMMIT;
}

if ($transactional)
{
#
--echo #
--echo # Transactional table locks do not interfere with the global read lock.
--echo # Take an exclusive lock on t1.
LOCK TABLE t1 IN EXCLUSIVE MODE;
    --echo # connection conn1.
    connection conn1;
    --echo # Try an exclusive lock, which conflicts.
    --error ER_LOCK_WAIT_TIMEOUT
    LOCK TABLE t1 IN EXCLUSIVE MODE;
    --echo # Can take the global read lock when an exclusive lock exist.
    FLUSH TABLES WITH READ LOCK;
    --echo # Show that the global read lock exists.
    --error ER_CANT_UPDATE_WITH_READLOCK
    LOCK TABLE t2 WRITE;
--echo # connection default.
connection default;
--echo # Can take an exclusive lock when the global read lock exists.
--echo # Take an exclusive lock on t2.
LOCK TABLE t2 IN EXCLUSIVE MODE;
    --echo # connection conn1.
    connection conn1;
    --echo # Show that an exclusive lock on t1 exists.
    --error ER_LOCK_WAIT_TIMEOUT
    LOCK TABLE t1 IN EXCLUSIVE MODE;
    --echo # Show that an exclusive lock on t2 exists.
    --error ER_LOCK_WAIT_TIMEOUT
    LOCK TABLE t2 IN EXCLUSIVE MODE;
    --echo # Show that the global read lock exists.
    --error ER_CANT_UPDATE_WITH_READLOCK
    LOCK TABLE t2 WRITE;
    --echo # Release global read lock.
    UNLOCK TABLES;
    --echo # Commit.
    COMMIT;
--echo # connection default.
connection default;
UNLOCK TABLES;
--echo # Commit.
COMMIT;
#
# Derived from a suggestion from Sergei:
--echo #
--echo # Access conflict on INSERT.
--echo # Take an share lock on t1.
LOCK TABLE t1 IN SHARE MODE;
    --echo # connection conn1.
    connection conn1;
    SELECT GET_LOCK("mysqltest1", 10);
    --echo # Try to insert a value,
    --echo # which must wait in background for the lock to go away.
    send INSERT INTO t1 VALUES (111);
--echo ## connection default.
connection default;
--echo ## Wait in background until the insert times out and releases lock.
send SELECT GET_LOCK("mysqltest1", 10);
    --echo # connection conn1.
    connection conn1;
    --echo # Wait for INSERT to timeout.
    --error ER_LOCK_WAIT_TIMEOUT
    reap;
    SELECT RELEASE_LOCK("mysqltest1");
    COMMIT;
--echo # connection default.
connection default;
reap;
--echo # Commit.
COMMIT;
--echo # Show that the insert in conn1 failed.
SELECT * FROM t1;
#
# Derived from a suggestion from Sinisa (concurrent.inc):
--echo #
--echo # Access conflict on UPDATE with exclusive lock.
--echo # Insert a value.
INSERT INTO t1 VALUES (111);
COMMIT;
    --echo # connection conn1.
    connection conn1;
    --echo # Take an exclusive lock.
    LOCK TABLE t1 IN EXCLUSIVE MODE;
--echo # connection default.
connection default;
--echo # Try a second exclusive lock, which fails due to the other lock.
--error ER_LOCK_WAIT_TIMEOUT
LOCK TABLE t1 IN EXCLUSIVE MODE;
--echo # Try an update, which fails due to the exclusive lock.
--error ER_LOCK_WAIT_TIMEOUT
UPDATE t1 SET c1= 111222;
    --echo # connection conn1.
    connection conn1;
    --echo # The exclusive table locker can still update.
    UPDATE t1 SET c1= 111333;
--echo # connection default.
connection default;
--echo # Select is allowed despite the table lock, but sees old data.
SELECT * FROM t1;
    --echo # connection conn1.
    connection conn1;
    --echo # Commit.
    COMMIT;
--echo # connection default.
connection default;
--echo # It seems like the failed update began a transaction, so still old data.
SELECT * FROM t1;
--echo # Commit.
COMMIT;
--echo # Now select sees current data.
SELECT * FROM t1;
TRUNCATE t1;
COMMIT;
--echo #
--echo # Access conflict on UPDATE with share lock.
--echo # Insert a value.
INSERT INTO t1 VALUES (111);
COMMIT;
    --echo # connection conn1.
    connection conn1;
    --echo # Take a share lock.
    LOCK TABLE t1 IN SHARE MODE;
    --echo # Update with a single share lock is possible.
    UPDATE t1 SET c1= 111222;
    --echo # Commit to get rid of the row lock.
    COMMIT;
    --echo # Take a share lock.
    LOCK TABLE t1 IN SHARE MODE;
--echo # connection default.
connection default;
--echo # An exclusive lock is not possible on a share lock.
--error ER_LOCK_WAIT_TIMEOUT
LOCK TABLE t1 IN EXCLUSIVE MODE;
--echo # More share locks are possible.
LOCK TABLE t1 IN SHARE MODE;
--echo # Noone can update when multiple share locks exist.
--error ER_LOCK_WAIT_TIMEOUT
UPDATE t1 SET c1= 111333;
    --echo # connection conn1.
    connection conn1;
    --echo # Noone can update when multiple share locks exist.
    --error ER_LOCK_WAIT_TIMEOUT
    UPDATE t1 SET c1= 111444;
    COMMIT;
--echo # connection default.
connection default;
SELECT * FROM t1;
TRUNCATE t1;
COMMIT;
#
}

--echo #
--echo # LOCK TABLE is prohibited in stored procedure.
--error ER_SP_BADSTATEMENT
CREATE PROCEDURE lock_t1_excl()
  LOCK TABLE t1 IN EXCLUSIVE MODE;
#
--echo #
--echo # LOCK TABLE is prohibited in trigger.
--error ER_SP_BADSTATEMENT
CREATE TRIGGER t1_ai AFTER INSERT ON t1 FOR EACH ROW
  LOCK TABLE t2 IN EXCLUSIVE MODE;
#

if ($transactional)
{
--echo #
--echo # LOCK TABLE on a pre-locked table through a trigger.
--echo # Create a trigger on t1 that updates t2.
DELIMITER //;
CREATE TRIGGER t1_ai AFTER INSERT ON t1 FOR EACH ROW
BEGIN
  UPDATE t2 SET c2= c2 + 111;
END//
DELIMITER ;//
--echo # Take an exclusive lock on t1.
--echo # This pre-locks t2 through the trigger.
LOCK TABLE t1 IN EXCLUSIVE MODE;
    --echo # connection conn1.
    connection conn1;
    --echo # Try to take an exclusive lock on t2,
    --echo # which is pre-locked through the trigger on t1.
    --error ER_LOCK_WAIT_TIMEOUT
    LOCK TABLE t2 IN EXCLUSIVE MODE;
    COMMIT;
--echo # connection default.
connection default;
--echo # Commit to release the lock.
COMMIT;
DROP TRIGGER t1_ai;
#
--echo #
--echo # LOCK TABLE on a pre-locked table through a view.
--echo # Create a function that selects from t2.
DELIMITER //;
CREATE FUNCTION count_t2() RETURNS INT
BEGIN
  RETURN (SELECT COUNT(*) FROM t2);
END//
DELIMITER ;//
--echo # Create a view with t1 and the function.
CREATE VIEW v1 AS SELECT COUNT(*), count_t2() FROM t1 GROUP BY 2;
--echo # Take an exclusive lock on v1.
--echo # This pre-locks t2 through the view.
LOCK TABLE v1 IN EXCLUSIVE MODE;
    --echo # connection conn1.
    connection conn1;
    --echo # Try to take an exclusive lock on t2,
    --echo # which is pre-locked through the function in v1.
    --error ER_LOCK_WAIT_TIMEOUT
    LOCK TABLE t2 IN EXCLUSIVE MODE;
    COMMIT;
--echo # connection default.
connection default;
--echo # Commit to release the lock.
COMMIT;
DROP VIEW v1;
DROP FUNCTION count_t2;
#
--echo #
--echo # Transactional LOCK TABLE by-passes pre-lock.
--echo #
--echo # Insert a value to t1 and t2 each.
INSERT INTO t1 VALUES (111);
INSERT INTO t2 VALUES (222);
COMMIT;
--echo # Create a trigger on t1 that updates t2.
DELIMITER //;
CREATE TRIGGER t1_ai AFTER INSERT ON t1 FOR EACH ROW
BEGIN
  INSERT INTO t2 SELECT GET_LOCK("mysqltest1", 10);
  UPDATE t2 SET c2= c2 + 111;
  INSERT INTO t2 SELECT RELEASE_LOCK("mysqltest1");
END//
DELIMITER ;//
--echo # Take an SQL lock which blocks the trigger.
SELECT GET_LOCK("mysqltest1", 10);
    --echo # connection conn1.
    connection conn1;
    --echo # Insert into t1 to fire trigger. This waits on GET_LOCK.
    send INSERT INTO t1 VALUES(111222);
--echo # connection default.
connection default;
--echo # Wait for the helper thread to sit on its lock.
while (`SELECT COUNT(*) < 1 FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE STATE LIKE '%lock%'`)
{
  --sleep 0.1
}
--echo # Take an exclusive lock.
LOCK TABLE t2 IN EXCLUSIVE MODE;
--echo # Use the lock for insert.
INSERT INTO t2 VALUES (111333);
--echo # Commit to release the lock again.
COMMIT;
--echo # Release the SQL lock to let the trigger finish.
SELECT RELEASE_LOCK("mysqltest1");
    --echo # connection conn1.
    connection conn1;
    --echo # Trigger succeeded.
    reap;
    --echo # Commit.
    COMMIT;
--echo # connection default.
connection default;
--echo # Commit.
COMMIT;
--echo # Show the results.
SELECT * FROM t1;
SELECT * FROM t2;
TRUNCATE t1;
TRUNCATE t2;
COMMIT;
DROP TRIGGER t1_ai;
#
--echo #
--echo # Non-transactional LOCK TABLE cannot by-passes pre-lock.
--echo #
--echo # Insert a value to t1 and t2 each.
INSERT INTO t1 VALUES (111);
INSERT INTO t2 VALUES (222);
COMMIT;
--echo # Create a trigger on t1 that updates t2.
DELIMITER //;
CREATE TRIGGER t1_ai AFTER INSERT ON t1 FOR EACH ROW
BEGIN
  # This will time out. So use a small value.
  INSERT INTO t2 SELECT GET_LOCK("mysqltest1", 1);
  UPDATE t2 SET c2= c2 + 111;
  INSERT INTO t2 SELECT RELEASE_LOCK("mysqltest1");
END//
DELIMITER ;//
--echo # Take an SQL lock which blocks the trigger.
SELECT GET_LOCK("mysqltest1", 10);
    --echo # connection conn1.
    connection conn1;
    --echo # Insert into t1 to fire trigger. This waits on GET_LOCK.
    send INSERT INTO t1 VALUES(111222);
--echo # connection default.
connection default;
--echo # Wait for the helper thread to sit on its lock.
while (`SELECT COUNT(*) < 1 FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE STATE LIKE '%lock%'`)
{
  --sleep 0.1
}
--echo # Take a write lock. This waits until the trigger times out.
LOCK TABLE t2 WRITE;
--echo # Use the lock for insert.
INSERT INTO t2 VALUES (111333);
--echo # Release the lock again.
UNLOCK TABLES;
--echo # Release the SQL lock.
SELECT RELEASE_LOCK("mysqltest1");
    --echo # connection conn1.
    connection conn1;
    --echo # Trigger timed out.
    --error ER_LOCK_WAIT_TIMEOUT
    reap;
    --echo # Commit.
    COMMIT;
--echo # connection default.
connection default;
--echo # Commit.
COMMIT;
--echo # Show the results.
SELECT * FROM t1;
SELECT * FROM t2;
TRUNCATE t1;
TRUNCATE t2;
COMMIT;
DROP TRIGGER t1_ai;
}

#
--echo ## Cleanup.
connection default;
SET AUTOCOMMIT= 1;
UNLOCK TABLES;
disconnect conn1;
disconnect conn2;
DROP TABLE t1, t2;
#

