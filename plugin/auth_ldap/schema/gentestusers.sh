#!/bin/bash
# Copyright (c) 2010 Edward "Koko" Konetzko <konetzed@quixoticagony.com>

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
