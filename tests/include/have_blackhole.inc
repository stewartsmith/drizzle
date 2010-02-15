disable_query_log;
--require r/true.require
select (is_active = 'TRUE') as `TRUE` from data_dictionary.plugins where PLUGIN_NAME = 'blackhole';
enable_query_log;
