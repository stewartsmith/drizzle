# 
# Test CREATE SCHEMA and replication
#

--disable_warnings
DROP SCHEMA IF EXISTS my_new_schema;
--enable_warnings

CREATE SCHEMA my_new_warnings;

DROP SCHEMA my_new_warnings;
