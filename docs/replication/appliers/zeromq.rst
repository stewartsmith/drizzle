.. _zeromq:

.. _zeromq_applier:

ZeroMQ Applier
==============

ZeroMQ is a messaging library that allows you to easily build complex
communication systems. The ZeroMQ plugin allows drizzle to publish
transactions to a local PUB socket. Many clients can subscribe to
these transactions. The first frame of the message sent out is the
schema name the transaction touched - this enables clients to only
subscribe to the interesting schemas (note that certain "transactions"
are without a schema, like SET xyz for example, for these, the first
frame is empty).

Getting started
---------------

First, install zeromq, get the code from `zeromq.org
<http://zeromq.org>`_, then you can build drizzle, watch the
./configure output to verify that drizzle finds the libraries needed.

Now you are good to go, simply start drizzle with --plugin-add=zeromq
and drizzle will start publishing transactions. The only configuration
parameter available is:

.. code-block:: bash

  --zeromq.endpoint arg (=tcp://*:9999) - the endpoint to expose.

Now you can write a simple python script to verify that it works,
something like this will work:

.. code-block:: python

  import zmq

  ctx = zmq.Context()
  s = ctx.socket(zmq.SUB)
  s.setsockopt(zmq.SUBSCRIBE, '')
  s.connect('tcp://localhost:9999')
  i = 0
  while True:
      i = i+1
      s.recv_multipart()
      print i

and then you can generate some load:

.. code-block:: bash

  bin/drizzleslap -c 10 -a --auto-generate-sql-add-autoincrement --burnin

which creates 10 threads and generates random queries.
