Installing From Source
======================

Dependencies
------------

Minimal Requirements
^^^^^^^^^^^^^^^^^^^^
To compile Drizzle with the most basic set of pluginsyou will need to following
dependencies installed:

 * autoconf
 * automake
 * bison
 * flex
 * gettext
 * gperf
 * intltool
 * libboost-date-time-dev
 * libboost-dev
 * libboost-filesystem-dev
 * libboost-iostreams-dev
 * libboost-program-options-dev
 * libboost-regex-dev
 * libboost-test-dev
 * libboost-thread-dev
 * libpcre3-dev
 * libreadline5-dev | libreadline-dev
 * libtool
 * protobuf-compiler
 * python-sphinx
 * uuid-dev
 * zlib1g-dev (>= 1:1.1.3-5)

Full Dependencies
^^^^^^^^^^^^^^^^^
Additionally, if you wish to build all of the plugins, you will need to install
these too:

 * libcurl4-gnutls-dev
 * libgcrypt11-dev
 * libgearman-dev (>= 0.10)
 * libhaildb-dev (>= 2.3.1)
 * libmemcached-dev (>= 0.39)
 * libpam0g-dev
 * libprotobuf-dev (>= 2.1.0)
 * libtokyocabinet-dev (>= 1.4.23)
 * systemtap-sdt-dev
 * libnotifymm-dev
 * doxygen
 * pandora-build

Obtaining The Source
--------------------
The latest source release can always be found on our `LaunchPad site
<https://launchpad.net/drizzle>`_, alternatively the bzr source from our stable
trunk can be obtained by doing::

   bzr branch lp:drizzle

Compiling The Source
--------------------
Compiling is as simple as doing the following inside the source::

   ./config/autorun.sh
   ./configure
   make
   make install

