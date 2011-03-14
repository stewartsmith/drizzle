###################################################
# Author: pcrews
# Date: 2011-03.09
# Purpose: To wait until the master and slave have
#         sync'ed - we do this by comparing the 
#         max applied id on the slave to the max
#         logged id on the master
#
# Requirements: we expect the master on default conn
#               slave on slave_con
# Details:
#      1) Fill in and setup variables
#      2) loop through looking for both
#         io and sql threads to start
#      3) If loops too long die.
####################################################
let $row_number= 1;
let $run= 1;
let $counter= 500;

while ($run)
{
  connection slave_con ;
  let $slave_max_id = query_get_value("SELECT last_applied_commit_id from sys_replication.applier_state", last_applied_commit_id, $row_number);
  connection default ;
  let $master_max_id = query_get_value("SELECT MAX(commit_id) from DATA_DICTIONARY.SYS_REPLICATION_LOG", MAX(commit_id), $row_number);
  if ($slave_max_id == $master_max_id)
    {
      let $run= 0;
    }
  sleep 0.1;
  if (!$counter){
    --echo "Failed while waiting for slave to sync with master"
    exit;
  }
  dec $counter;
}


