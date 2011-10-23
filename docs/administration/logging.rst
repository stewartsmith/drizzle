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

Since :program:`syslog` is the standard UNIX logging facility,
the :ref:`syslog_plugin` plugin is also loaded by default which will log error
messages to the system log (:file:`/var/log/syslog` on Ubuntu for example).
However, you must start :program:`drizzled` with
:option:`--syslog.errmsg-enable` to enable this feature.

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
