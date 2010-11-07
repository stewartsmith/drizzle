disable_query_log;
--require r/true.require
select (is_active = 'YES') as `TRUE` from data_dictionary.plugins where PLUGIN_NAME = 'csv';
enable_query_log;
