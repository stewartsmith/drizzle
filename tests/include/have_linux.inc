-- require r/have_linux.require
disable_query_log;
select VARIABLE_VALUE from data_dictionary.global_variables WHERE Variable_name="version_compile_os";
enable_query_log;
