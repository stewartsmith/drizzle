# Replication tests need binlog
source include/have_log_bin.inc;

connect (master,localhost,root,,test,$MASTER_MYPORT,);
connect (master1,localhost,root,,test,$MASTER_MYPORT,);
connect (slave,localhost,root,,test,$SLAVE_MYPORT,);
connect (slave1,localhost,root,,test,$SLAVE_MYPORT,);

-- source include/master-slave-reset.inc

# Set the default connection to 'master'
connection master;
