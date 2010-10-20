#!/bin/sh
# Copyright (C) 2010 Eric Day

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
