## Bug#12713 (Error in a stored function called from a SELECT doesn't cause
##    ROLLBACK of statem)

##
## Pre-Requisites :
## - $engine_type should be set
##

eval set storage_engine = $engine_type;
set autocommit=1;

--disable_warnings
drop table if exists t1;
drop table if exists t2;
drop table if exists t3;
--enable_warnings

create table t1 (a int);
create table t2 (a int unique);
create table t3 (a int);

insert into t1 (a) values (1), (2);
insert into t3 (a) values (1), (2);

##============================================================================
## Design notes
##
## In each case, statement rollback is expected.
## for transactional engines, the rollback should be properly executed
## for non transactional engines, the rollback may cause warnings.
##
## The test pattern is as follows
## - insert 1000+N
## - statement with a side effect, that fails to insert N twice
## - a statement rollback is expected (expecting 1 row 1000+N only) in t2
## - a rollback is performed
## - expecting a clean table t2.
##============================================================================

insert into t2 (a) values (1001);
insert into t1 (a) values (1);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1002);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1003);
update t1 set a= a + 3;
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1004);
update t1, t3 set t1.a = 0, t3.a = 0 where (4 = 4) and (t1.a = t3.a);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1005);
delete from t1 where (a = 5);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1006);
delete from t1, t3 using t1, t3 where (6 = 6) ;
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1007);
replace t1 values (7);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1008);
replace into t3 (a) select 8 from t1;
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1009);
select 9 from t1 ;
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1010);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1011);
select * from t2;
rollback;
select * from t2;

select * from t2;

insert into t2 (a) values (1013);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1014);
show open tables;
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1015);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1016);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1017);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1018);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1019);
select * from t2;
rollback;
select * from t2;

select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1021);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1022);
select * from t2;
rollback;
select * from t2;

insert into t2 (a) values (1023);
select * from t2;
rollback;
select * from t2;

--echo =======================================================================
--echo Testing select_to_file
--echo =======================================================================

insert into t2 (a) values (1025);

--replace_result $MYSQLTEST_VARDIR ..
eval select 25 into outfile "$MYSQLTEST_VARDIR/tmp/dml.out" from t1;
select * from t2;
rollback;
select * from t2;
--remove_file $MYSQLTEST_VARDIR/tmp/dml.out

insert into t2 (a) values (1026);
--replace_result $MYSQLTEST_VARDIR ..
--error 1366
eval load data infile "../std_data_ln/words.dat" into table t1 (a) set a:=26;

select * from t2;
rollback;
select * from t2;

--echo =======================================================================
--echo Testing select_dumpvar
--echo =======================================================================

insert into t2 (a) values (1027);
select 27 into @foo;
select * from t2;
rollback;
select * from t2;

--echo =======================================================================
--echo Cleanup
--echo =======================================================================

set autocommit=default;

drop table t1;
drop table t2;
drop table t3;
--echo #
--echo # Bug#12713 Error in a stored function called from a SELECT doesn't
--echo # cause ROLLBACK of statem
--echo #
--echo # Verify that two-phase commit is not issued for read-only
--echo # transactions.
--echo #
--echo # Verify that two-phase commit is issued for read-write transactions,
--echo # even if the change is done inside a stored function called from
--echo # SELECT or SHOW statement.
--echo #
set autocommit=0;
--disable_warnings
drop table if exists t1;
drop table if exists t2;
drop table if exists t3;
--enable_warnings
