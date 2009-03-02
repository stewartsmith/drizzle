################################################################################
# inc/vcol_blocked_sql_funcs_main.inc                                          #
#                                                                              #
# Purpose:                                                                     #
#  Tests around sql functions                                                  #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-08-31                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

#
# NOTE:  All SQL functions should be rejected, otherwise BUG.
#

--echo # RAND()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (b virtual double as (rand()));

--echo # LOAD_FILE()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(64), b virtual varchar(1024) as (load_file(a)));

--echo # CURDATE()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual datetime as (curdate()));

--echo # CURRENT_DATE(), CURRENT_DATE
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual datetime as (current_date));
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual datetime as (current_date()));

--echo # CURRENT_TIMESTAMP(), CURRENT_TIMESTAMP
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual datetime as (current_timestamp()));
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual datetime as (current_timestamp));

--echo # LOCALTIMESTAMP, LOCALTIMESTAMP()(v4.0.6)
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b virtual varchar(10) as (localtimestamp()));
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b virtual varchar(10) as (localtimestamp));

--echo # NOW()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b virtual varchar(10) as (now()));

--echo # SYSDATE()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b virtual varchar(10) as (sysdate()));

--echo # UNIX_TIMESTAMP()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b virtual datetime as (unix_timestamp()));

--echo # UTC_DATE()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b virtual datetime as (utc_date()));

--echo # UTC_TIMESTAMP()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b virtual datetime as (utc_timestamp()));

--echo # BENCHMARK()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b virtual varchar(1024) as (benchmark(a,3)));

--echo # CONNECTION_ID()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual int as (connection_id()));

--echo # CURRENT_USER(), CURRENT_USER
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual varchar(32) as (current_user()));
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual varchar(32) as (current_user));

--echo # DATABASE()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b virtual varchar(1024) as (database()));

--echo # FOUND_ROWS()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b virtual varchar(1024) as (found_rows()));

--echo # LAST_INSERT_ID()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual int as (last_insert_id()));

--echo # ROW_COUNT()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual int as (row_count()));

--echo # SCHEMA()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual varchar(32) as (schema()));

--echo # SESSION_USER()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual varchar(32) as (session_user()));

--echo # SYSTEM_USER()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual varchar(32) as (system_user()));

--echo # USER()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b virtual varchar(1024) as (user()));

--echo # UUID()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a virtual varchar(1024) as (uuid()));

--echo # VALUES()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b virtual varchar(1024) as (values(a)));

--echo # VERSION()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b virtual varchar(1024) as (version())); 

--echo # Unknown functions
-- error ER_SP_DOES_NOT_EXIST
create table t1 (a virtual int as (f1()));

--echo # WEIGHT_STRING()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(5), b virtual varchar(10) as (weight_string(a as char(4))));

--echo #
--echo # GROUP BY FUNCTIONS
--echo #

--echo # AVG()
-- error 1111
create table t1 (a int, b virtual int as (avg(a)));

--echo # COUNT(DISTINCT)
-- error 1111
create table t1 (a int, b virtual int as (count(distinct a)));

--echo # COUNT()
-- error 1111
create table t1 (a int, b virtual int as (count(a)));

--echo # GROUP_CONCAT()
-- error 1111
create table t1 (a varchar(32), b virtual int as (group_concat(a,'')));

--echo # MAX()
-- error 1111
create table t1 (a int, b virtual int as (max(a)));

--echo # MIN()
-- error 1111
create table t1 (a int, b virtual int as (min(a)));

--echo # STD()
-- error 1111
create table t1 (a int, b virtual int as (std(a)));

--echo # STDDEV_POP()
-- error 1111
create table t1 (a int, b virtual int as (stddev_pop(a)));

--echo # STDDEV_SAMP()
-- error 1111
create table t1 (a int, b virtual int as (stddev_samp(a)));

--echo # STDDEV()
-- error 1111
create table t1 (a int, b virtual int as (stddev(a)));

--echo # SUM()
-- error 1111
create table t1 (a int, b virtual int as (sum(a)));

--echo # VAR_POP()
-- error 1111
create table t1 (a int, b virtual int as (var_pop(a)));

--echo # VAR_SAMP()
-- error 1111
create table t1 (a int, b virtual int as (var_samp(a)));

--echo # VARIANCE()
-- error 1111
create table t1 (a int, b virtual int as (variance(a)));

--echo #
--echo # Sub-selects
--echo #

create table t1 (a int);
-- error 1064
create table t2 (a int, b virtual int as (select count(*) from t1));
drop table t1;

--echo #
--echo # Long expression

let $tmp_long_string = `SELECT repeat('a',240)`;
eval create table t1 (a int, b virtual varchar(300) as (concat(a,'$tmp_long_string')));
drop table t1;
let $tmp_long_string = `SELECT repeat('a',243)`;
--error 1470
eval create table t1 (a int, b virtual varchar(300) as (concat(a,'$tmp_long_string')));

--echo #
--echo # Constant expression
--error ER_CONST_EXPR_IN_VCOL
create table t1 (a virtual int as (1));
