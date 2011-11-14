.. program:: drizzled

.. _drizzled_process:

drizzled Process
================

.. _starting_and_stopping_drizzled:

Starting and Stopping
---------------------

Although :program:`drizzled` can be started without any command line options
or config files, you will certainly want to specify a minimal configuration
to suite your particular system.  You should be familiar with the
:ref:`drizzled_configuration` options and variables.

Managing the :program:`drizzled` process depends on whether Drizzle
was installed from a package or source.  Package installations can use
the operating system's service managment programs, but source installations
require simple scripts to manage Drizzle manually.

Ubuntu
^^^^^^

Ubuntu uses the :program:`service` program to manage services.  To start
and stop Drizzle from the command line:

.. code-block:: bash

   $ sudo service drizzle start
   drizzle start/running, process 1431

   $ sudo service drizzle stop
   drizzle stop/waiting

:file:`/etc/init/drizzle.conf` controls how :program:`service` starts
and stops Drizzle.

Manually
^^^^^^^^

If Drizzle was installed from source, or you want to manage
:program:`drizzled` manually, you should create a script
to start Drizzle from the command line.  For example, if Drizzle was
installed to :file:`/usr/local/drizzle`, this script will start Drizzle
with a minimal configuration:

.. code-block:: bash

   #!/bin/sh

   BASEDIR="/usr/local/drizzle"

   cd $BASEDIR

   ./sbin/drizzled                     \
      --basedir=$BASEDIR               \
      --datadir=$BASEDIR/data          \
      --pid-file=/var/run/drizzled.pid \
   > $BASEDIR/var/log/drizzled.log 2>&1 &

See :ref:`configuring_drizzle` for more information about setting additional
command line options.

Use the :ref:`drizzle_command_line_client` to stop Drizzle:

.. code-block:: bash

   $ drizzle --shutdown

Or, execute ``shtudown``:

.. code-block:: mysql

   drizzle> shutdown;

The ``shutdown`` command is case-sensitive.

Output and Logging
------------------

:program:`drizzled` does *not* close or redirect output to ``STDOUT`` or
``STDERR`` to a log file or logging facility like :program:`syslog`.
When starting Drizzle, you should redirect ``STDOUT`` and ``STDERR`` to a log 
file or to /dev/null, as in the above script example when starting Drizzle.

When running :program:`drizzled` manually from a console, just allowing the
output to be printed for you can of course be useful.

Note that beginning with the first ``Drizzle 7.1 Beta 2011.10.28`` drizzled
will also log messages via syslog and this is *in addition* to using ``STDOUT``
and ``STDERR``.


Signaling
---------

====== ========
Signal Response
====== ========
HUP    Ignore
TERM   Shutdown
====== ========

