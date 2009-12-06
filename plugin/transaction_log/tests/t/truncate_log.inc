# 
# Simple test of the serial event log for testing the truncation 
# of the event log via the @transaction_log_truncate_debug variable.
#

SET GLOBAL transaction_log_truncate_debug= true;
