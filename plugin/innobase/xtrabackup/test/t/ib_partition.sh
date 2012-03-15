# ib_partition.sh - test for xtrabackup
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

OUTFILE=results/ib_partition_innobackupex.out

init
run_mysqld --innodb_file_per_table
load_dbase_schema part_range_sample

echo "
[mysqld]
datadir=$mysql_datadir" > $topdir/my.cnf


vlog "Adding initial rows to database..."
numrow=500
count=0
while [ "$numrow" -gt "$count" ]
do
        ${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($count);" part_range_sample
        let "count=count+1"
done
vlog "Initial rows added"

checksum_a=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" part_range_sample | awk '{print $2}'`
vlog "checksum_a is $checksum_a"

echo "part_range_sample.test" > $topdir/list

# partial backup of partitioned table
mkdir -p $topdir/data/full
vlog "Starting backup"
innobackupex-1.5.1 --user=root --socket=$mysql_socket --defaults-file=$topdir/my.cnf --tables-file=$topdir/list $topdir/data/full > $OUTFILE 2>&1 || die "innobackupex-1.5.1 died with exit code $?"
backup_dir=`grep "innobackupex-1.5.1: Backup created in directory" $OUTFILE | awk -F\' '{ print $2}'`
vlog "Partial backup done"

# Prepare backup
innobackupex-1.5.1 --defaults-file=$topdir/my.cnf --apply-log $backup_dir
vlog "Log applied to backup"

# removing rows
vlog "Table cleared"
${MYSQL} ${MYSQL_ARGS} -e "delete from test;" part_range_sample

# Restore backup

stop_mysqld

vlog "Copying files"
rm -rf $mysql_datadir/part_range_sample
innobackupex-1.5.1 --copy-back --defaults-file=$topdir/my.cnf $backup_dir
vlog "Data restored"

run_mysqld --innodb_file_per_table

vlog "Cheking checksums"
checksum_b=`${MYSQL} ${MYSQL_ARGS} -Ns -e "checksum table test;" part_range_sample | awk '{print $2}'`
vlog "checksum_b is $checksum_b"

if [ $checksum_a -eq $checksum_b ]
then 
        vlog "Checksums are OK"
        stop_mysqld
        clean
        exit 0
else
        vlog "Checksums are not equal"
        exit -1
fi
