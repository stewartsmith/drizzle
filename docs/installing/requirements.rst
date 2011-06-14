Requirements
============

Supported Platforms
-------------------
When installing Drizzle we typically recommend either using the packages from
a Linux distribution or packages provided by our own repositories.  This is
because the dependencies required by Drizzle can sometimes be too old or missing
in some Linux distributions.

Every build of Drizzle is currently tested on:
 * Fedora 12 through 14
 * RedHat (or CentOS) 5 and 6
 * Debian Squeeze (6.0)
 * Ubuntu 10.04 and 10.10
 * Apple OSX 10.6.4

We recommend using these platforms with Drizzle. Older platforms may have various
unforseen difficulties when compiling and/or installing.  Drizzle is tested on
both 32bit and 64bit platforms but we recommend using a 64bit platform.

Software Requirements
---------------------
To install and use Drizzle you will need the following basic packages:

 * `Boost <http://www.boost.org/>`_ 1.40 or higher
 * `Google Protocol Buffers <http://code.google.com/apis/protocolbuffers/>`_
 * libuuid (part of the `E2fsprogs <http://e2fsprogs.sourceforge.net/>`_ project)
 * `zlib <http://www.zlib.net/>`_ 1.1.3-5 or higher

Our repositories for RedHat/CentOS/Fedora and for Ubuntu will provide these where required.
