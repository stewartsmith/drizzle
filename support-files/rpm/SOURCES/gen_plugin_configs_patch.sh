#!/bin/bash
#  Copyright (C) 2011 BJ Dierkes
#  All rights reserved.
# 
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
# 
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
# 
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#
#
#
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
