#!/usr/bin/env bash
#
#  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2011 Lee Bieber
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#   * Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#   * Neither the name of Patrick Galbraith nor the names of its contributors
#     may be used to endorse or promote products derived from this software
#     without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.
#
#

export RABBITMQ_NODENAME="drizzle_test"
export RABBITMQ_NODE_IP_ADDRESS="0.0.0.0"

DIR=$RABBITMQ_NODENAME
rm -rf $DIR
mkdir -p $DIR/logs
mkdir $DIR/mnesia

export RABBITMQ_MNESIA_BASE="`pwd`/$DIR/mnesia"
export RABBITMQ_LOG_BASE="`pwd`/$DIR/logs"


startup()
{
   /usr/lib/rabbitmq/bin/rabbitmq-server -detached
   sleep 5
}

shutdown()
{
  /usr/lib/rabbitmq/bin/rabbitmqctl -q -n $RABBITMQ_NODENAME stop
  sleep 5
  rm -rf $DIR
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

