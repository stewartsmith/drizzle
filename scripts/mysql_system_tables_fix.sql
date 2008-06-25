# This part converts any old privilege tables to privilege tables suitable
# for current version of MySQL

# You can safely ignore all 'Duplicate column' and 'Unknown column' errors
# because these just mean that your tables are already up to date.
# This script is safe to run even if your tables are already up to date!

# On unix, you should use the mysql_fix_privilege_tables script to execute
# this sql script.
# On windows you should do 'mysql --force mysql < mysql_fix_privilege_tables.sql'

set storage_engine=MyISAM;


# Activate the new, possible modified privilege tables
# This should not be needed, but gives us some extra testing that the above
# changes was correct

flush privileges;
