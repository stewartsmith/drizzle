Installing From Source
======================

* :ref:`dependencies`
* :ref:`ubuntu-install`
* :ref:`debian-install`
* :ref:`obtain-source`
* :ref:`compile-source`

.. _dependencies:

Dependencies
------------

Minimal Requirements
^^^^^^^^^^^^^^^^^^^^
To compile Drizzle with the most basic set of plugins, you will need to have the following
dependencies installed. Scroll down for the apt-get install commands for Ubuntu and Debian.

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
these additional dependencies:

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

.. _ubuntu-install:

Ubuntu Install Commands
-------------------------

The following commands work on Ubuntu to install the tools and libraries needed to build Drizzle:

.. code-block:: bash

  sudo apt-get install python-software-properties
  sudo add-apt-repository ppa:drizzle-developers/ppa
  sudo apt-get update
  sudo apt-get install drizzle-dev

.. _debian-install:

Debian Install Commands
-------------------------

Since apt-repository isn't in Debian, you can instead add the Maverick PPA to /etc/apt/sources.list as follows:

Add the following lines to /etc/apt/sources.list (make sure it's two
lines): ::

	deb http://ppa.launchpad.net/drizzle-developers/ppa/ubuntu maverick main
	deb-src http://ppa.launchpad.net/drizzle-developers/ppa/ubuntu maverick main

Add the signing key to your keyring: ::

	sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 06899068

Then run:

.. code-block:: bash

  apt-get update; apt-get install drizzle-dev

Note that these instructions are only for Debian Squeeze. For current
unstable/testing (aka Wheezy), the recommended source for Drizzle is the
main repository. In other words:

.. code-block:: bash

  apt-get install drizzle-dev

or (if you want to install and not build drizzle):

.. code-block:: bash

  apt-get install drizzle 

.. _obtain-source:

Obtaining The Source
--------------------
The latest source release can always be found on our `LaunchPad site
<https://launchpad.net/drizzle>`_. Alternatively, the bzr source from our stable
trunk can be obtained by doing:

.. code-block:: bash

   bzr branch lp:drizzle

.. _compile-source:

Compiling The Source
--------------------
Compiling is done by performing the standard automake commands from the top level directory inside the source:

.. code-block:: bash

   ./config/autorun.sh
   ./configure
   make
   make install

