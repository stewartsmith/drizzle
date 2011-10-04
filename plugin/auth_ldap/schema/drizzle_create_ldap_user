#!/bin/bash
#
# Copyright (C) 2010 Edward "Koko" Konetzko <konetzed@quixoticagony.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#
#     * The names of its contributors may not be used to endorse or
# promote products derived from this software without specific prior
# written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

help() {
    echo "Arguement options are:"
    echo "-p: password to use for password"
    echo "-b: path to mysql_password_hash"
    echo "-u: username to generate users from"
    echo "-n: number of users to generate"
    echo "-l: base ldap dn to use for user generation"
    echo "-d: debug"
    echo 
    echo "$0 is a script used to generate users to test"
    echo " drizzles mysql auth integration with ldap."
    echo " if \"-b\" is set users will be generated with attribute"
    echo " drizzleUserMysqlPassword"
    echo " Script dumps all information to stdout so end user can decide"
    echo " what they want to do with output."
    echo 
}

genldif() {
    if [ $debug ] 
    then 
        echo "-p: $password"
        echo "-b: $mysqlpasswordhashbin"
        echo "-u: $username"
        echo "-n: $numberofusers"
        echo "-l: $ldapbase"
        echo "-d: $debug" 
        echo 
    fi

    tmpcount=0
    while [ $tmpcount -lt $numberofusers ]
    do
        tmpusername=$username$tmpcount
        tmpuidnumber=$(( 500 + $tmpcount ))
        tmpgidnumber=$(( 500 + $tmpcount ))
        echo "dn: uid=$tmpusername,$ldapbase"
        echo "objectclass: top"
        echo "objectclass: posixAccount"
        echo "objectclass: account"
        if [ $mysqlpasswordhashbin ]
        then
            echo "objectclass: drizzleUser"
            mysqlpasshash=`$mysqlpasswordhashbin $password`
            echo "drizzleUserMysqlPassword: $mysqlpasshash"
        fi
        echo "uidNumber: $tmpuidnumber"
        echo "gidNumber: $tmpgidnumber"
        echo "uid: $tmpusername"
        echo "homeDirectory: /home/$tmpusername"
        echo "loginshell: /sbin/nologin"
        echo "userPassword: $password"
        echo "cn: $tmpusername"
        echo
        tmpcount=$(($tmpcount + 1))

    done
}

while [ $# -gt 0 ]
do
    case $1
    in
        -p)
            password=$2
            shift 2
        ;;
        
        -b)
            mysqlpasswordhashbin=$2
            shift 2
        ;;
        
        -u)
            username=$2
            shift 2
        ;;
        
        -n)
            numberofusers=$2
            shift 2
        ;;
        -l)
            ldapbase=$2
            shift 2
        ;;
        -d)
            debug=1
            shift 1
        ;;

        *)
            help
            shift 1
            exit
        ;;
    esac
done

genldif
