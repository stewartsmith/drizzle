#
# Bug #27877 incorrect german order in utf8_general_ci
#
# Testing if "SHARP S" is equal to "S",
# like in latin1_german1_ci, utf8_general_ci, ucs2_general_ci
# Or if "SHART S" is equal to "SS",
# like in latin1_german2_ci, utf8_unicode_ci, ucs2_unicode_ci
# 
# Also testing A-uml, O-uml, U-uml
#

--disable_warnings
drop table if exists t1;
--enable_warnings

#
# Create a table with a varchar(x) column,
# using current values of
# @@character_set_connection and  @@collation_connection.
#

create table t1 as select repeat(' ', 64) as s1;
select collation(s1) from t1;
delete from t1;

#
# Populate data
#

insert into t1 values ('a'),('ae'),('ä');
insert into t1 values ('o'),('oe'),('ö');
insert into t1 values ('s'),('ss'),('ß');
insert into t1 values ('u'),('ue'),('ü');

#
# Check order
#
select s1, hex(s1) from t1 order by s1, binary s1;
select group_concat(s1 order by binary s1) from t1 group by s1;
drop table t1;
