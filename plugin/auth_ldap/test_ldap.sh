#!/bin/sh
# Copyright (C) 2010 Eric Day
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

# Kill any leftover processes from before
pids=`ps -ef|grep drizzled|grep 12345|awk '{print $2}'`
if [ "$pids" != "" ]
then
  kill -9 $pids
  sleep 1
fi

datadir=$1

./drizzled/drizzled \
  --datadir=$datadir \
  --plugin-add=auth_ldap \
  --auth-ldap-uri=ldap://127.0.0.1:12321/ \
  --auth-ldap-bind-dn="cn=root,dc=drizzle,dc=org" \
  --auth-ldap-bind-password=testldap \
  --auth-ldap-base-dn="dc=drizzle,dc=org" \
  --auth-ldap-cache-timeout=1 \
  --mysql-protocol-port=12345 \
  --drizzle-protocol.port=12346 \
  --pid-file=pid &

sleep 3

failed=0

for x in 1 2
do
  echo
  echo "Test good login:"
  echo "SELECT 'SUCCESS';" | ./client/drizzle -u user -Pdrizzlepass -p 12345
  if [ $? -ne 0 ]
  then
    failed=1
  fi

  echo
  echo "Test bad password:"
  echo "SELECT 'FAIL';" | ./client/drizzle -u user -Pbadpass -p 12345
  if [ $? -ne 1 ]
  then
    failed=1
  fi

  echo
  echo "Test no password:"
  echo "SELECT 'FAIL';" | ./client/drizzle -u user -p 12345
  if [ $? -ne 1 ]
  then
    failed=1
  fi

  echo
  echo "Test bad user:"
  echo "SELECT 'FAIL';" | ./client/drizzle -u baduser -Pdrizzlepass -p 12345
  if [ $? -ne 1 ]
  then
    failed=1
  fi
  echo

  echo "Test bad user with no password:"
  echo "SELECT 'FAIL';" | ./client/drizzle -u baduser -p 12345
  if [ $? -ne 1 ]
  then
    failed=1
  fi

  # sleep here so ldap cache has time to clear
  sleep 2
done

kill `cat $datadir/pid`
sleep 2

echo

if [ $failed -ne 0 ]
then
  echo "At least one test failed, see error messages above"
  exit 1
fi

echo "All tests passed successfully!"
exit 0
