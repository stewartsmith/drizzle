# check_transaction_log.inc
# a test-run include file that will check the transaction log contents
# through a variety of means.
# For a given query we will check the data_dictoionary views:
#      transaction_log
#      transaction_log_entries
#      transaction_log_transactions
# We will also check the transaction log contents via the 
# udf - PRINT_TRANSACTION_MESSAGE(transaction_log, <offset>)

#--echo # check transaction_log
#--replace_column 7 START_TIMESTAMP 8 END_TIMESTAMP 9 INDEX_SIZE
#SELECT COUNT(*) FROM DATA_DICTIONARY.TRANSACTION_LOG;
#--echo

--echo Check transaction_log_entries
SELECT COUNT(*) FROM DATA_DICTIONARY.TRANSACTION_LOG_ENTRIES;
--echo

--echo Check transaction_log_transactions
--replace_column 4 START_TIMESTAMP 5 END_TIMESTAMP
SELECT COUNT(*) FROM DATA_DICTIONARY.TRANSACTION_LOG_TRANSACTIONS;

--echo Check transaction log contents
--replace_regex /start_timestamp: [0-9]+/START_TIMESTAMP/g /end_timestamp: [0-9]+/END_TIMESTAMP/g /creation_timestamp: [0-9]+/CREATE_TIMESTAMP/g /update_timestamp: [0-9]+/UPDATE_TIMESTAMP/g /uuid: [-a-zA-Z0-9"]+/UUID/g /transaction_id: [0-9]+/TRANSACTION_ID/

SELECT PRINT_TRANSACTION_MESSAGE('transaction.log',(select max(entry_offset) from DATA_DICTIONARY.TRANSACTION_LOG_TRANSACTIONS));
--echo
COMMIT;
