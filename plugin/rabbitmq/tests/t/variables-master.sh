#! /bin/sh

echo "RABBITMQ_NODE_ PORT = $RABBITMQ_NODE_PORT"
echo "Stopping rabbitmq"
sudo -E $TOP_SRCDIR/plugin/rabbitmq/admin.sh stop
sleep 5
echo "Starting rabbitmq"
sudo -E $TOP_SRCDIR/plugin/rabbitmq/admin.sh start

