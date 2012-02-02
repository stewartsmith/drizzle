#!/bin/bash

# This is a temporary script that takes local conf.d configs stored in bzr
# but not via the SRPM, and generates a plugin-configs.patch which is
# used via the SPEC.
#
# Will be deprecated by:
#
#   https://bugs.launchpad.net/drizzle/+bug/712194
#

if [ "x$1" == "x" ]; then
    echo "Drizzle version required as argument"
    exit 1
fi

version=$1

spectool -g ../SPECS/drizzle.spec 

rm -f plugin-configs.patch
rm -rf drizzle-${version}/
tar -zxf drizzle-${version}.tar.gz

for i in $(ls conf.d | sed 's/.cnf//g'); do
    _name=$(echo $i | sed 's/-/_/g')
    touch drizzle-${version}/plugin/${_name}/plugin.cnf.orig
    cp -a conf.d/${i}.cnf drizzle-${version}/plugin/${_name}/plugin.cnf  
    diff -Naur drizzle-${version}/plugin/${_name}/plugin.cnf.orig \
        drizzle-${version}/plugin/${_name}/plugin.cnf >> plugin-configs.patch 
done

echo "wrote ./plugin-configs.patch"
