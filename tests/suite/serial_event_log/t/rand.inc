# 
# Simple test of the serial event log for testing rand() call 
# 
# We then use the transaction_reader in drizzled/message/ to read the events.
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL
, PRIMARY KEY (id)
);

insert into t1 values (1);
insert into t1 select rand(100)*100 from t1;

# Read in the event.log.  We must cut out all the Timestamp: XXXXXX
# because they are not deterministic. The XID numbers are also removed
# these are deterministic, but they introduce a test dependancy making
# it difficult to add/remove tests in the future. 
--exec ../drizzled/message/command_reader var/master-data/event.log | tail -n 8 | sed 's/\/\* SID: [0-9] XID: [0-9]* \*\/ //g' | sed 's/Timestamp: [0-9]*//g'
