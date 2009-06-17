# 
# Simple test of the serial event log for testing TRUNCATE command 
# 
# We create a table and insert some data then call truncate on the 
# table.
# 
# We then use the transaction_reader in drizzled/message/ to read the events.
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (
  id INT NOT NULL
, padding VARCHAR(200) NOT NULL
);

INSERT INTO t1 VALUES (1, "I love testing.");
INSERT INTO t1 VALUES (2, "I hate testing.");

# TRUNCATE

SELECT * FROM t1;
TRUNCATE TABLE t1;
SELECT * FROM t1;

# Read in the event.log.  We must cut out all the Timestamp: XXXXXX
# because they are not deterministic. The XID numbers are also removed
# these are deterministic, but they introduce a test dependancy making
# it difficult to add/remove tests in the future.
--exec ../drizzled/message/command_reader var/master-data/event.log | tail -n 10 | sed 's/\/\* SID: [0-9] XID: [0-9]* \*\/ //g' | sed 's/Timestamp: [0-9]*//g'
