Installing In Ubuntu
====================

Using DEBs
----------

Ubuntu Natty (11.04) has Drizzle .deb files in the repositories. For Ubuntu 10.04 and 10.10 we have a PPA available at
https://launchpad.net/~drizzle-developers/+archive/ppa

As a first step, run the following command: ::

	sudo apt-get install python-software-properties

To add the above PPA at command line simply run: ::

	sudo apt-add-repository ppa:drizzle-developers/ppa
	sudo apt-get update

To then install Drizzle: ::

	sudo apt-get install drizzle drizzle-client
