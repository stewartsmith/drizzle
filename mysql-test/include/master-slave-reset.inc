connection slave;

#we expect STOP SLAVE to produce a warning as the slave is stopped
#(the server was started with skip-slave-start)
--disable_warnings
stop slave;
--wait_for_slave_to_stop
--enable_warnings

connection master;
--disable_warnings
--disable_query_log
use test;
--enable_query_log
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings
reset master;
connection slave;

--disable_warnings
# the first RESET SLAVE may produce a warning about non-existent
# 'ndb_apply_status' table, because this table is created
# asynchronously at the server startup and may not exist yet
# if RESET SLAVE comes too soon after the server startup
reset slave;
--enable_warnings

# Clean up old test tables
--disable_warnings
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings

--disable_query_log
eval CHANGE MASTER TO MASTER_USER='root',
                      MASTER_CONNECT_RETRY=1,
                      MASTER_HOST='127.0.0.1',
                      MASTER_PORT=$MASTER_MYPORT;
--enable_query_log

start slave;
