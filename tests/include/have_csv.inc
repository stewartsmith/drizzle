disable_query_log;
--require r/true.require
select (is_active = 'YES') as `TRUE` from information_schema.plugins where PLUGIN_NAME = 'csv';
enable_query_log;
