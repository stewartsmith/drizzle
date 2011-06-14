#!/bin/bash
# run.sh - test runner for xtrabackup
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

result=0
rm -rf results
mkdir results
function usage()
{
	echo "Usage: $0 [-m mysql_version] [-g] [-h]"
	echo "-m version  MySQL version to use. Possible values: system, 5.0, 5.1, 5.5, percona. Default is system"
	echo "-g          Output debug information to results/*.out"
	echo "-t          Run only a single named test"
	echo "-h          Print this help message"
	echo "-s	  Select a test suite to run. Possible values: experimental, t. Default is t"
}
XTRACE_OPTION=""
export SKIPPED_EXIT_CODE=200
export MYSQL_VERSION="system"
while getopts "gh?m:t:s:" options; do
	case $options in
		m ) export MYSQL_VERSION="$OPTARG";;
		t ) tname="$OPTARG";;
		g ) XTRACE_OPTION="-x";;
		h ) usage; exit;;
		s ) tname="$OPTARG/*.sh";;
		\? ) usage; exit -1;;
		* ) usage; exit -1;;
	esac
done

if [ -n "$tname" ]
then
   tests="$tname"
else
   tests="t/*.sh"
fi

for t in $tests
do
   printf "%-40s" $t
   bash $XTRACE_OPTION $t > results/`basename $t`.out 2>&1
   rc=$?
   if [ $rc -eq 0 ]
   then
       echo "[passed]"
   elif [ $rc -eq $SKIPPED_EXIT_CODE ]
   then
       echo "[skipped]"
   else
       echo "[failed]"
       result=1
   fi
done

if [ $result -eq 1 ]
then
    echo "There are failing tests!!!"
    echo "See results/ for detailed output"
    exit -1
fi
