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

#
# Test when using replace on table with multiple unique keys
# and a primary key of various data types. This tests code
# that, during a REPLACE, deletes multiple conflicting rows.
# (FYI, In the transaction log, there should be 2 DELETEs
# and 1 UPDATE for each of these REPLACE statements.)
#

create table t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b char(1), c INT, d INT, unique key(b), unique key(c));
insert into t1 values (1,"a",1,1),(2,"b",2,2),(3,"c",3,3);
replace into t1 values (1,"b",3,4);
select * from t1 order by a;
drop table t1;

create table t1 (a CHAR(5) NOT NULL PRIMARY KEY, b char(1), c INT, d INT, unique key(b), unique key(c));
insert into t1 values ("a","a",1,1),("bb","b",2,2),("ccc","c",3,3);
replace into t1 values ("a","b",3,4);
select * from t1 order by a;
drop table t1;

create table t1 (a DATE NOT NULL PRIMARY KEY, b char(1), c INT, d INT, unique key(b), unique key(c));
insert into t1 values ("2010-01-01","a",1,1),("2010-02-02","b",2,2),("2010-03-03","c",3,3);
replace into t1 values ("2010-01-01","b",3,4);
select * from t1 order by a;
drop table t1;

create table t1 (a DOUBLE NOT NULL PRIMARY KEY, b char(1), c INT, d INT, unique key(b), unique key(c));
insert into t1 values (1.1,"a",1,1),(22.22,"b",2,2),(333.333,"c",3,3);
replace into t1 values (1.1,"b",3,4);
select * from t1 order by a;
drop table t1;

create table t1 (a FLOAT NOT NULL PRIMARY KEY, b char(1), c INT, d INT, unique key(b), unique key(c));
insert into t1 values (1.1,"a",1,1),(22.22,"b",2,2),(333.333,"c",3,3);
replace into t1 values (1.1,"b",3,4);
select * from t1 order by a;
drop table t1;

create table t1 (a ENUM("a","bb","ccc") NOT NULL PRIMARY KEY, b char(1), c INT, d INT, unique key(b), unique key(c));
insert into t1 values ("a","a",1,1),("bb","b",2,2),("ccc","c",3,3);
replace into t1 values ("a","b",3,4);
select * from t1 order by a;
drop table t1;
