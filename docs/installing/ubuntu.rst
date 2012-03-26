Installing on Ubuntu
=====================

The preferred way of installing Drizzle on Ubuntu is to use the provided deb
packages. 

Ubuntu 11.04 Natty Narwhal and newer comes with Drizzle .deb files in the 
standard Ubuntu repositories. You can install Drizzle without any further 
configuration.

Using DEBs
----------

For Ubuntu 10.04 LTS Lucid Lynx and Ubuntu 10.10 Maverick Meerkat there is a 
PPA with Drizzle 7 binaries at
https://launchpad.net/~drizzle-developers/+archive/ppa

As a first step, run the following command: ::

	sudo apt-get install python-software-properties

To add the PPA: ::

	sudo apt-add-repository ppa:drizzle-developers/ppa
	sudo apt-get update

The same PPA is also used by drizzle-developers team to publish newer versions
of Drizzle than what is included in Ubuntu at the time of release. For instance,
at the moment when this was written, Drizzle 7.1.31-rc was available for Ubuntu 
11.10 Oneiric, which by default includes Drizzle 7 packages. Hence you can
use this PPA on any Ubuntu version to update to the newest available Drizzle
version.

Installing
----------

To install Drizzle, both the server and the client utilities: ::

	sudo apt-get install drizzle
