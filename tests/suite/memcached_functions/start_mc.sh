#! /bin/sh

PORT=19191
if [ $MC_PORT ]; then
  PORT=$MC_PORT
fi

USER=nobody
MAXCONN=1024
CACHESIZE=1024
OPTIONS=""

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin:$PATH
RETVAL=0

start () {
	memcached -d -p $PORT -u $USER -m $CACHESIZE -c $MAXCONN -U 0
	RETVAL=$?
}
stop () {
	kill -9 `ps -ef | grep memcached | grep $PORT | grep $USER | grep -v grep | awk '{print $2}'`
        RETVAL=$?
}

restart () {
        stop
        start
}


# See how we were called.
case "$1" in
  start)
        start
        ;;
  stop)
        stop
        ;;
  restart|reload)
        restart
        ;;
  *)
        echo $"Usage: $0 {start|stop|restart|reload}"
        exit 1
esac

exit $?

