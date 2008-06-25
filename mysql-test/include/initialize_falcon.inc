disable_query_log;
SET @@autocommit = 1;
SET names default;
SET GLOBAL query_cache_size = 0;
SET @@sql_mode = '';
SET @@storage_engine = MyISAM;
SET @@tx_isolation = 'REPEATABLE-READ';
enable_query_log;
