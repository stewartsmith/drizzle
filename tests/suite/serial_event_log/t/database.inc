# 
# Simple test of the serial event log for CREATE DATABASE and
# DROP DATABASE  
# 
# Create a database then drop a database 
# 
# We then use the transaction_reader in drizzled/message/ to read the events.
#

# CREATE DATABASE 
CREATE DATABASE test1;
DROP DATABASE test1;

# Read in the event.log.  We must cut out all the Timestamp: XXXXXX
# because they are not deterministic. The XID numbers are also removed
# these are deterministic, but they introduce a test dependancy making
# it difficult to add/remove tests in the future.
--exec ../drizzled/message/command_reader var/master-data/event.log | tail -n 4 | sed 's/\/\* SID: [0-9] XID: [0-9]* \*\/ //g' | sed 's/Timestamp: [0-9]*//g'
