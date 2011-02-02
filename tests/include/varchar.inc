# Initialise
--disable_warnings
drop table if exists t1,t2,t3;
--enable_warnings

disable_query_log;
select "--- Testing varchar ---";
enable_query_log;

#
# Simple basic test that endspace is saved
#

eval create $temp table t1 (v varchar(10), c char(10), t text);
insert into t1 values('+ ', '+ ', '+ ');
set @a=repeat(' ',20);
--error ER_DATA_TOO_LONG
insert into t1 values (concat('+',@a),concat('+',@a),concat('+',@a));
set @a=repeat(' ',10);
--error ER_DATA_TOO_LONG
insert into t1 values (concat('+',@a),concat('+',@a),concat('+',@a));
set @a=repeat(' ',9);
insert into t1 values (concat('+',@a),concat('+',@a),concat('+',@a));
select concat('*',v,'*',c,'*',t,'*') from t1;

# Check how columns are copied
show create table t1;
eval create $temp table t2 like t1;
show create table t2;
eval create $temp table t3 select * from t1;
show create table t3;
alter table t1 modify c varchar(10);
show create table t1;
alter table t1 modify v char(10);
show create table t1;
alter table t1 modify t varchar(10);
show create table t1;
select concat('*',v,'*',c,'*',t,'*') from t1;
drop table t1,t2,t3;

#
# Testing of keys
#
eval create $temp table t1 (v varchar(10), c char(10), t text, key(v), key(c), key(t(10)));
show create table t1;
disable_query_log;
begin;
let $1=10;
while ($1)
{
  let $2=27;
  eval set @space=repeat(' ',10-$1);
  while ($2)
  {
    eval set @char=char(ascii('a')+$2-1);
    insert into t1 values(concat(@char,@space),concat(@char,@space),concat(@char,@space));
    dec $2;
  }
  dec $1;
}
commit;
enable_query_log;
select count(*) from t1;
insert into t1 values(concat('a',char(1)),concat('a',char(1)),concat('a',char(1)));
select count(*) from t1 where v='a';
select count(*) from t1 where c='a';
select count(*) from t1 where t='a';
select count(*) from t1 where v='a  ';
select count(*) from t1 where c='a  ';
select count(*) from t1 where t='a  ';
select count(*) from t1 where v between 'a' and 'a ';
select count(*) from t1 where v between 'a' and 'a ' and v between 'a  ' and 'b\n';
select count(*) from t1 where v like 'a%';
select count(*) from t1 where c like 'a%';
select count(*) from t1 where t like 'a%';
select count(*) from t1 where v like 'a %';
# Test results differ for BDB, see comments in bdb.test
# and they are also different from MySAM test results.
--replace_column 9 #
explain select count(*) from t1 where v='a  ';
--replace_column 9 #
explain select count(*) from t1 where c='a  ';
--replace_column 9 #
explain select count(*) from t1 where t='a  ';
--replace_column 9 #
explain select count(*) from t1 where v like 'a%';
--replace_column 9 #
explain select count(*) from t1 where v between 'a' and 'a ';
--replace_column 9 #
explain select count(*) from t1 where v between 'a' and 'a ' and v between 'a  ' and 'b\n';

--error ER_DUP_ENTRY
alter table t1 add unique(v);
alter table t1 add key(v);
select concat('*',v,'*',c,'*',t,'*') as qq from t1 where v='a';
--replace_column 6 # 9 #
explain select * from t1 where v='a';

# GROUP BY

select v,count(*) from t1 group by v limit 10;
select v,count(t) from t1 group by v limit 10;
select v,count(c) from t1 group by v limit 10;
select sql_big_result v,count(t) from t1 group by v limit 10;
select sql_big_result v,count(c) from t1 group by v limit 10;
select c,count(*) from t1 group by c limit 10;
select c,count(t) from t1 group by c limit 10;
select sql_big_result c,count(t) from t1 group by c limit 10;
select t,count(*) from t1 group by t limit 10;
select t,count(t) from t1 group by t limit 10;
select sql_big_result t,count(t) from t1 group by t limit 10;

#
# Test varchar > 255 bytes
#

alter table t1 modify v varchar(300), drop key v, drop key v_2, add key v (v);
show create table t1;
select count(*) from t1 where v='a';
select count(*) from t1 where v='a  ';
select count(*) from t1 where v between 'a' and 'a ';
select count(*) from t1 where v between 'a' and 'a ' and v between 'a  ' and 'b\n';
select count(*) from t1 where v like 'a%';
select count(*) from t1 where v like 'a %';
--replace_column 9 #
explain select count(*) from t1 where v='a  ';
--replace_column 9 #
explain select count(*) from t1 where v like 'a%';
--replace_column 9 #
explain select count(*) from t1 where v between 'a' and 'a ';
--replace_column 9 #
explain select count(*) from t1 where v between 'a' and 'a ' and v between 'a  ' and 'b\n';
--replace_column 9 #
explain select * from t1 where v='a';

# GROUP BY

select v,count(*) from t1 group by v limit 10;
select v,count(t) from t1 group by v limit 10;
select sql_big_result v,count(t) from t1 group by v limit 10;

#
# Test varchar > 255 bytes, key < 255
#

alter table t1 drop key v, add key v (v(30));
show create table t1;
select count(*) from t1 where v='a';
select count(*) from t1 where v='a  ';
select count(*) from t1 where v between 'a' and 'a ';
select count(*) from t1 where v between 'a' and 'a ' and v between 'a  ' and 'b\n';
select count(*) from t1 where v like 'a%';
select count(*) from t1 where v like 'a %';
--replace_column 9 #
explain select count(*) from t1 where v='a  ';
--replace_column 9 #
explain select count(*) from t1 where v like 'a%';
--replace_column 9 #
explain select count(*) from t1 where v between 'a' and 'a ';
--replace_column 9 #
explain select count(*) from t1 where v between 'a' and 'a ' and v between 'a  ' and 'b\n';
--replace_column 9 #
explain select * from t1 where v='a';

# GROUP BY

select v,count(*) from t1 group by v limit 10;
select v,count(t) from t1 group by v limit 10;
select sql_big_result v,count(t) from t1 group by v limit 10;

#
# Test varchar > 512 (special case for GROUP BY becasue of
# CONVERT_IF_BIGGER_TO_BLOB define)
#

alter table t1 modify v varchar(600), drop key v, add key v (v);
show create table t1;
select v,count(*) from t1 group by v limit 10;
select v,count(t) from t1 group by v limit 10;
select sql_big_result v,count(t) from t1 group by v limit 10;

drop table t1;

#
# Test unique keys
#

eval create $temp table t1 (a char(10), unique (a));
insert into t1 values ('a   ');
--error ER_DUP_ENTRY
insert into t1 values ('a ');

alter table t1 modify a varchar(10);
--error ER_DUP_ENTRY
insert into t1 values ('a '),('a  '),('a   '),('a         ');
--error ER_DUP_ENTRY
insert into t1 values ('a     ');
--error ER_DATA_TOO_LONG
insert into t1 values ('a          ');
--error ER_DUP_ENTRY
insert into t1 values ('a ');
update t1 set a='a  ' where a like 'a%';
select concat(a,'.') from t1;
update t1 set a='abc    ' where a like 'a ';
select concat(a,'.') from t1;
update t1 set a='a      ' where a like 'a %';
select concat(a,'.') from t1;
update t1 set a='a  ' where a like 'a      ';
select concat(a,'.') from t1;
drop table t1;

#
# test show create table
#

eval create $temp table t1 (v varchar(10), c char(10), t text, key(v(5)), key(c(5)), key(t(5)));
show create table t1;
drop table t1;
eval create $temp table t1 (v char(10));
show create table t1;
drop table t1;

eval create $temp table t1 (v varchar(10), c char(10));
show create table t1;
insert into t1 values('a','a'),('a ','a ');
select concat('*',v,'*',c,'*') from t1;
drop table t1;

#
# Test long varchars
#
# @TODO The below fails because it assumes latin1
# as the charset.  Possibly re-enable a similar test
# for UTF8-only in future
#create $temp table t1 (v varchar(65530), key(v(10)));
#insert into t1 values(repeat('a',65530));
#select length(v) from t1 where v=repeat('a',65530);
#drop table t1;

#
# Bug #9489: problem with hash indexes
# Bug #10802: Index is not used if table using BDB engine on HP-UX
#

eval create $temp table t1(a int, b varchar(12), key ba(b, a));
insert into t1 values (1, 'A'), (20, NULL);
explain select * from t1 where a=20 and b is null;
select * from t1 where a=20 and b is null;
drop table t1;
