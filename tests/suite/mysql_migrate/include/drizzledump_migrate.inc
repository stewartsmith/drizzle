# drizzledump_migrate.inc
# include file for the mysql_migrate suite of the Drizzle test-suite
# intended to assist with testing drizzledump's ability to migrate
# from MySQL databases.
#
# Requires:
# * Running MySQL database
# * ENVIRONMENT variables to be set
#   - DRIZZLE_MYSQL_MIGRATE_TEST
#   - DRIZZLE_MYSQL_MIGRATE_PORT
#
# The file will:
# * Send a specified file to be executed on the MySQL server
#   - the file is expected to be valid SQL and should be
#     CREATE TABLE / INSERTs to create something to migrate
# * Call drizzledump from the system under test to populate
#   the Drizzle server
# * Validation of the datasets via queries against Drizzle
#   are left to the individual cases 

# test if we should run this test, will skip if we don't have
# the environment variable set
--disable_query_log
--require r/true.require
SELECT VARIABLE_VALUE AS `TRUE` FROM DATA_DICTIONARY.ENVIRONMENTAL WHERE VARIABLE_NAME="DRIZZLE_MYSQL_MIGRATE_TEST";
--enable_query_log

--disable_warnings
eval DROP SCHEMA IF EXISTS $database;
--enable_warnings

# clean up our MySQL server
--echo Dropping test database on MySQL...
exec $DRIZZLE_CLIENT -uroot --port=$DRIZZLE_MYSQL_MIGRATE_PORT test -e "DROP SCHEMA IF EXISTS $database";

--echo Create test database on MySQL...
exec $DRIZZLE_CLIENT -uroot --port=$DRIZZLE_MYSQL_MIGRATE_PORT test -e "CREATE SCHEMA $database";

--echo populating MySQL with test data...
--echo $DRIZZLE_CLIENT -uroot --port=$DRIZZLE_MYSQL_MIGRATE_PORT $database < $datafile
exec $DRIZZLE_CLIENT -uroot --port=$DRIZZLE_MYSQL_MIGRATE_PORT $database < $datafile;

--echo calling drizzledump to populate Drizzle...
exec $DRIZZLE_DUMP_CLIENT --compact --host=127.0.0.1 --port=$DRIZZLE_MYSQL_MIGRATE_PORT --destination-type=database --destination-host=localhost --destination-port=$MASTER_MYPORT --destination-user=root --destination-database=$database --user=root $database ;

