# ib_include.sh - test for xtrabackup
# Copyright (C) 2009-2011 Percona Inc.

# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

. inc/common.sh

init
run_mysqld
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "create database include;"
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "create table t1 (a int) ENGINE=MyISAM;" include
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "create table t2 (a int) ENGINE=InnoDB;" include
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "insert into t1 values (1),(2),(3);" include
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "insert into t2 values (1),(2),(3);" include
# Take backup
mkdir -p $topdir/backup
checksum_t1=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table t1" include | awk '{print $2}'`
checksum_t2=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table t2" include | awk '{print $2}'`
vlog "checksum_t1 is $checksum_t1"
vlog "checksum_t2 is $checksum_t2"
run_cmd ${IB_BIN} --user=root --socket=$mysql_socket --include="^include[.]t" $topdir/backup > $OUTFILE 2>&1 
backup_dir=`grep "innobackupex: Backup created in directory" $OUTFILE | awk -F\' '{ print $2}'`
stop_mysqld
# Remove datadir
rm -r $mysql_datadir
# Restore data
vlog "Applying log"
echo "###########" >> $OUTFILE
echo "# PREPARE #" >> $OUTFILE
echo "###########" >> $OUTFILE
run_cmd ${IB_BIN} --apply-log $backup_dir >> $OUTFILE 2>&1
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
echo "###########" >> $OUTFILE
echo "# RESTORE #" >> $OUTFILE
echo "###########" >> $OUTFILE
run_cmd ${IB_BIN} --copy-back $backup_dir >> $OUTFILE 2>&1
run_mysqld
checksum_tt1=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table t1" include | awk '{print $2}'`
checksum_tt2=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table t2" include | awk '{print $2}'`
vlog "checksum_tt1 is $checksum_tt1"
vlog "checksum_tt2 is $checksum_tt2"
if [ (( ($checksum_t2 -eq $checksum_tt2) || ($checksum_t1 -eq $checksum_tt1) )) ]
then
	vlog "Checksums are OK"
else
        vlog "Checksums are not equal"
        exit -1
fi
stop_mysqld
clean
