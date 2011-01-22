#! /bin/sh

echo "Stopping rabbitmq"
$TOP_SRCDIR/plugin/rabbitmq/admin.sh stop
sleep 5
echo "Starting rabbitmq"
$TOP_SRCDIR/plugin/rabbitmq/admin.sh start
sleep 5
