#!/usr/bin/env bash

PORT=19191
if [ $MC_PORT ]; then
  PORT=$MC_PORT
fi

MAXCONN=1024
CACHESIZE=1024
OPTIONS=""

startup()
{
  memcached -d -p $PORT -m $CACHESIZE -c $MAXCONN -U 0 -P /tmp/memc.pid.$PORT
}

shutdown()
{
  if [ -f /tmp/memc.pid.$PORT ]
  then
    kill -9 `cat /tmp/memc.pid.$PORT`
    rm /tmp/memc.pid.$PORT
  fi
}

restart()
{
  shutdown
  startup
}


# See how we were called.
case "$1" in
  start)
        startup
        ;;
  stop)
        shutdown
        ;;
  restart|reload)
        restart
        ;;
  *)
        echo $"Usage: $0 {start|stop|restart|reload}"
        exit 1
esac

exit $?

