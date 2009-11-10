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
