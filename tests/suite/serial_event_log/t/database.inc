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
