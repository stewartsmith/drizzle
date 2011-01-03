# clean up our MySQL server
--echo Dropping test database on MySQL...
exec $DRIZZLE_CLIENT -uroot --port=$DRIZZLE_MYSQL_MIGRATE_PORT test -e "DROP SCHEMA IF EXISTS $database";

