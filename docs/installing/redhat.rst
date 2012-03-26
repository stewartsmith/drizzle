Installing on RedHat and Fedora
===============================

The recommended way to install Drizzle on RedHat, CentOS and Fedora is to use
`yum` to install Drizzle RPM packages.

There is a custom RedHat/Fedora repository available which includes everything 
needed to install or compile Drizzle on RedHat 5 and 6 as well as 
Fedora 13 - 14. For Drizzle 7, the repositories are at

http://rpm.drizzle.org/7-dev/ 

and for Drizzle 7.1 at

http://download.drizzle.org/7.1/

EPEL repository (RedHat only)
-----------------------------

A pre-requisite of this repository in RedHat is that the
`EPEL <http://fedoraproject.org/wiki/EPEL>`_ repository is also added to your
distribution.

.. note::

   The EPEL repository is not required in Fedora.

To add the EPEL repository, run one of the following commands (choose from the 
i386(32-bit) or x86_64(64-bit) EPEL repository package).

Install EPEL Repository on 32-bit Linux 5.X:

.. code-block:: bash

  sudo rpm -Uvh http://download.fedora.redhat.com/pub/epel/5/i386/epel-release-5-4.noarch.rpm

Install EPEL Repository on 64-bit Linux 5.X:

.. code-block:: bash

  sudo rpm -Uvh http://download.fedora.redhat.com/pub/epel/5/x86_64/epel-release-5-4.noarch.rpm

Install EPEL Repository on 32-bit Linux 6.X:

.. code-block:: bash

  sudo rpm -Uvh http://download.fedoraproject.org/pub/epel/6/i386/epel-release-6-5.noarch.rpm

Install EPEL Repository on 64-bit Linux 6.X:

.. code-block:: bash

  sudo rpm -Uvh http://download.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-5.noarch.rpm


Adding the Drizzle 7 yum repository
-----------------------------------

.. note::

    This section provides the repositories for Drizzle 7.

To add the Drizzle repository to your system you also need to create a 
/etc/yum.repos.d/drizzle.repo file with the following content:

*RedHat*

.. code-block:: ini

   [drizzle]
   name=drizzle
   baseurl=http://rpm.drizzle.org/7-dev/redhat/$releasever/$basearch/
   enabled=1
   gpgcheck=0

   [drizzle-src]
   name=drizzle-src
   baseurl=http://rpm.drizzle.org/7-dev/redhat/$releasever/source
   enabled=1
   gpgcheck=0

**$releasever** uses RHEL 5 or RHEL 6, and **$basearch** is the architecture 
(i386 or x86_64).

*Fedora*

.. code-block:: ini

   [drizzle]
   name=drizzle
   baseurl=http://rpm.drizzle.org/7-dev/fedora/$releasever/$basearch/
   enabled=1
   gpgcheck=0

   [drizzle-src]
   name=drizzle-src
   baseurl=http://rpm.drizzle.org/7-dev/fedora/$releasever/source
   enabled=1
   gpgcheck=0

**$releasever** uses the Fedora version (currently 13 or 14), and **$basearch** 
is the architecture (i386 or x86_64).

You can then install Drizzle by running the following command:

.. code-block:: bash

   sudo yum install drizzle-server drizzle-client


Adding the Drizzle 7.1 yum repository
-------------------------------------

.. note::

    This section provides the repositories for Drizzle 7.1.

To add the Drizzle repository to your system you also need to create a 
/etc/yum.repos.d/drizzle.repo file with the following content:

*RedHat*

.. code-block:: ini

   [drizzle]
   name=drizzle
   baseurl=http://download.drizzle.org/7.1/redhat/$releasever/$basearch/
   enabled=1
   gpgcheck=0

   [drizzle-src]
   name=drizzle-src
   baseurl=http://download.drizzle.org/7.1/redhat/$releasever/source
   enabled=1
   gpgcheck=0

**$releasever** uses RHEL 5 or RHEL 6, and **$basearch** is the architecture 
(i386 or x86_64).

*Fedora*

.. code-block:: ini

   [drizzle]
   name=drizzle
   baseurl=http://download.drizzle.org/7.1/fedora/$releasever/$basearch/
   enabled=1
   gpgcheck=0

   [drizzle-src]
   name=drizzle-src
   baseurl=http://download.drizzle.org/7.1/fedora/$releasever/source
   enabled=1
   gpgcheck=0

**$releasever** uses the Fedora version (currently 13 or 14), and **$basearch** 
is the architecture (i386 or x86_64).

.. note::

    At the time of this writing, Fedora packages were not yet released for 
    Drizzle 7.1 series.


Installation
------------

You can then install Drizzle by running the following command:

.. code-block:: bash

   sudo yum install drizzle-server drizzle-client

Note: In the Drizzle 7-stable repository, the packages are named 
drizzle7-server and drizzle7-client.

