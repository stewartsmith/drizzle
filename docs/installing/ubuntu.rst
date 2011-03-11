Installing In Ubuntu
====================

Using DEBs
----------

Ubuntu 11.04 Natty Narwhal has Drizzle .deb files in the standard Ubuntu repositories.
For Ubuntu 10.04 LTS Lucid Lynx and Ubuntu 10.10 Maverick Meerkat there is a PPA available at
https://launchpad.net/~drizzle-developers/+archive/ppa

As a first step, run the following command: ::

	sudo apt-get install python-software-properties

To add the above PPA at command line: ::

	sudo apt-add-repository ppa:drizzle-developers/ppa
	sudo apt-get update

Then to install Drizzle, both the server and the client utilities: ::

	sudo apt-get install drizzle
