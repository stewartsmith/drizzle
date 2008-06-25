--disable_query_log
--require r/true.require
select (plugin_status = 'ACTIVE') as `TRUE` from information_schema.plugins where PLUGIN_NAME = 'archive';
--enable_query_log
