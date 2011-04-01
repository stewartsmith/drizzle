# xb_stats.sh - test for xtrabackup
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
load_dbase_schema sakila
load_dbase_data sakila

# Take backup
mkdir -p $topdir/backup
run_cmd xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/backup
vlog "Backup taken, trying stats"
run_cmd xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/backup

# First check that xtrabackup fails with the correct error message
# when trying to get stats before creating the log files

vlog "===> xtrabackup --stats --datadir=$topdir/backup"
if xtrabackup --stats --datadir=$topdir/backup >$OUTFILE 2>&1
then
    die "xtrabackup --stats was expected to fail, but it did not."
fi
if ! grep "Cannot find log file ib_logfile0" $OUTFILE
then
    die "Cannot find the expected error message from xtrabackup --stats"
fi

# Now create the log files in the backup and try xtrabackup --stats again

run_cmd xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/backup

run_cmd xtrabackup --stats --datadir=$topdir/backup

vlog "stats did not fail"

stop_mysqld
clean
