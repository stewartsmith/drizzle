--disable_warnings
DROP DATABASE IF EXISTS bup_ddl_blocker;
--enable_warnings

CREATE DATABASE bup_ddl_blocker;

# Create transaction tables and load them with data.
--echo con1: Creating tables
CREATE TABLE bup_ddl_blocker.t1 (col_a CHAR(40)) ENGINE=INNODB;
CREATE TABLE bup_ddl_blocker.t2 (col_a CHAR(40)) ENGINE=MYISAM;
CREATE TABLE bup_ddl_blocker.t3 (col_a CHAR(40)) ENGINE=INNODB;
CREATE TABLE bup_ddl_blocker.t4 (col_a CHAR(40)) ENGINE=MYISAM;

--echo con1: Loading data
INSERT INTO bup_ddl_blocker.t1 VALUES ("01 Some data to test");
INSERT INTO bup_ddl_blocker.t1 VALUES ("02 Some data to test");
INSERT INTO bup_ddl_blocker.t1 VALUES ("03 Some data to test");

INSERT INTO bup_ddl_blocker.t2 VALUES ("01 Some data to test");
INSERT INTO bup_ddl_blocker.t2 VALUES ("02 Some data to test");
INSERT INTO bup_ddl_blocker.t2 VALUES ("03 Some data to test");

INSERT INTO bup_ddl_blocker.t3 VALUES ("01 Some data to test");
INSERT INTO bup_ddl_blocker.t3 VALUES ("02 Some data to test");
INSERT INTO bup_ddl_blocker.t3 VALUES ("03 Some data to test");

INSERT INTO bup_ddl_blocker.t4 VALUES ("01 Some data to test");
INSERT INTO bup_ddl_blocker.t4 VALUES ("02 Some data to test");
INSERT INTO bup_ddl_blocker.t4 VALUES ("03 Some data to test");

