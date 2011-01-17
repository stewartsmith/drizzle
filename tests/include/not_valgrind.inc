-- require r/not_valgrind.require
disable_query_log;
select COUNT(*) as c from data_dictionary.environmental WHERE Variable_name="VALGRIND_RUN";
enable_query_log;
