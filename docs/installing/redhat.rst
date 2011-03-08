Installing in RedHat and Fedora
================================

Using RPMs
----------
There is a custom RedHat/Fedora repository at
http://rpm.drizzle.org/7-dev/ which includes everything needed
to install or compile Drizzle on RedHat/CentOS 5 and 6 as well as on Fedora 12 through 14.

On Redhat/CentOS systems, but not on Fedora systems, you must add the 
`EPEL reposatory<http://fedoraproject.org/wiki/EPEL>`_
to your RPM configuration, by running the following command:

.. code-block:: bash

  sudo rpm -Uvh http://download.fedora.redhat.com/pub/epel/5/i386/epel-release-5-4.noarch.rpm

To add the Drizzle repository to your system you also need to create a /etc/yum.repos.d/drizzle.repo file with the following content:

*RedHat*

.. code-block:: ini

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

*Fedora*

.. code-block:: ini

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

You can then install Drizzle by running the following command:

.. code-block:: bash

   sudo yum install drizzle7-server drizzle7-client

Compiling From Source
---------------------
To compile from source you will need to add the repositories described above, and then install the following packages:

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
