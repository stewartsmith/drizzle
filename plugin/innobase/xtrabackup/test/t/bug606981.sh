# bug606981.sh - test for xtrabackup
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

OUTFILE=results/bug606981_innobackupex_out

init
run_mysqld
load_sakila

# Take backup
echo "
[mysqld]
datadir=$mysql_datadir
innodb_flush_method=O_DIRECT
" > $topdir/my.cnf
mkdir -p $topdir/backup
innobackupex --user=root --socket=$mysql_socket --defaults-file=$topdir/my.cnf --stream=tar $topdir/backup > $topdir/backup/out.tar 2>$OUTFILE || dir "innobackupex died with exit code $?"
stop_mysqld

# See if tar4ibd was using O_DIRECT
if ! grep "tar4ibd: using O_DIRECT for the input file" $OUTFILE ;
then
  vlog "tar4ibd was not using O_DIRECT for the input file."
  exit -1
fi

# Remove datadir
rm -r $mysql_datadir
# Restore sakila
vlog "Applying log"
backup_dir=$topdir/backup
cd $backup_dir
tar -ixvf out.tar
cd - >/dev/null 2>&1 
run_cmd innobackupex --apply-log --defaults-file=$topdir/my.cnf $backup_dir
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
run_cmd innobackupex --copy-back --defaults-file=$topdir/my.cnf $backup_dir

run_mysqld
# Check sakila
${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
stop_mysqld
clean
