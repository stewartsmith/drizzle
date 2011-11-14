.. program:: drizzled

Logging
=======

Error Messages
--------------

Error messages are also handled by plugins.  Drizzle loads the
:ref:`errmsg_stderr_plugin` plugin by default which prints error messages
to ``STDERR``.  In many cases, this is sufficient if ``STDERR`` is
redirected to a log file when
:ref:`starting Drizzle <starting_and_stopping_drizzled>`.

Note that beginning with the first ``Drizzle 7.1 Beta 2011.10.28`` drizzled
will **also** log messages via syslog and this is *in addition* to using 
``STDOUT`` and ``STDERR``. This functionality is provided by the 
:ref:`syslog_plugin` plugin. (For example on Ubuntu these messages are written
to :file:`/var/log/syslog`).

Queries
-------

The :ref:`query_log_plugin` plugin is the most feature-complete and flexible
query logging plugin.  It logs queries to a file with a consistent,
easy-to-parse format, and all of its options are dynamic, so once the plugin
is loaded, logging can be enabled and disabled at runtime.

Drizzle does not load the :ref:`query_log_plugin` plugin by default, so you
must load it when starting Drizzle.

The :ref:`syslog_plugin` plugin also provides basic query logging to
the system log.  Although the plugin is loaded by default, you must
enable query logging by specifying :option:`--syslog.logging-enable`
and one of the threshold options, such as
:option:`--syslog.logging-threshold-slow`.  The plugin is not dynamic,
so once query logging is enabled, you must restart Drizzle to disable
or change its settings.
