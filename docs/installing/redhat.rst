Installing in RedHat and Fedora
===============================

Using RPMs
----------
We have a custom RedHat/Fedora repository at
http://rpm.drizzle.org/7-dev/ which includes everything needed
to install or compile Drizzle on RedHat 5 and 6 as well as Fedora 13 - 14.

A pre-requisite of this repository in RedHat is that the
`EPEL <http://fedoraproject.org/wiki/EPEL>`_ repository is also added to your
distribution.

.. note::

   The EPEL repository is not required in Fedora.

To add that repository, run the following command::

  sudo rpm -Uvh http://download.fedora.redhat.com/pub/epel/5/i386/epel-release-5-4.noarch.rpm

To add the repository to your distribution you also need to create a /etc/yum.repos.d/drizzle.repo file with the following content:

*RedHat*::

   [drizzle]
   name=drizzle
   baseurl=http://rpm.drizzle.org/7-dev/redhat/$releasever/$basearch/
   enabled=1
   gpgcheck=0

   [drizzle-src]
   name=drizzle-src
   baseurl=http://rpm.drizzle.org/7-dev/redhat/$releasever/SRPMS
   enabled=1
   gpgcheck=0

*Fedora*::

   [drizzle]
   name=drizzle
   baseurl=http://rpm.drizzle.org/7-dev/fedora/$releasever/$basearch/
   enabled=1
   gpgcheck=0

   [drizzle-src]
   name=drizzle-src
   baseurl=http://rpm.drizzle.org/7-dev/fedora/$releasever/SRPMS
   enabled=1
   gpgcheck=0

You can then use the following shell command::

   yum install drizzle7

Compiling From Source
---------------------
To compile from source you will need to add the repositories above and then install the following packages:

 * bzr
 * boost-devel
 * autoconf
 * automake
 * gcc
 * gcc-c++
 * libtool
 * gperf
 * libuuid-devel (part of e2fsprogs-devel on older RedHat based distributions)
 * zlib-devel
 * pcre-devel
 * readline-devel
 * flex
 * bison

You will then be able to compile from source in the normal way.
