#
# This once provided compatibility for innodb_rollback_on_timeout, now it is
# just a test feeder file for innodb-myisam
#
#
--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1 (a int not null primary key) engine = innodb;
insert into t1 values (1);
commit;
connect (con1,localhost,root,,);
connect (con2,localhost,root,,);

connection con2;
begin work;
insert into t1 values (2);
select * from t1;

connection con1;
begin work;
insert into t1 values (5);
select * from t1;
# Lock wait timeout set to 2 seconds in <THIS TEST>-master.opt; this
# statement will time out; in 5.0.13+, it will not roll back transaction.
--error ER_LOCK_WAIT_TIMEOUT
insert into t1 values (2);
# On 5.0.13+, this should give ==> 1, 5
select * from t1;
commit;

connection con2;
select * from t1;
commit;

connection default;
select * from t1;
drop table t1;
disconnect con1;
disconnect con2;
