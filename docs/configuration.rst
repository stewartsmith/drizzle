=============
Configuration
=============

--------
Overview
--------

Drizzle can draw its configuration from a number of sources, including the
command line, configuration files and environment variables.

Support is planned for pluggable configuration souces.

----------------
Loading Sequence
----------------

Drizzle first reads all of the command line, as these options may also
include information about where configuration files should be found.
