#! /usr/bin/python
# -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010 Patrick Crews
#
"""port_management.py
   code for dealing with the various tasks
   around handing out and managing server ports
   that we need to run tests

"""

# imports
import os
import sys

class portManager:
    """ class for doing the work of handing out and tracking ports """
    def __init__(self, system_manager, debug = 0):
        # This is a file that can be read into a dictionary
        # it is in port:owner format
        self.skip_keys = [ 'port_file_delimiter'
                         , 'system_manager'
                         ]
        self.port_catalog = "/tmp/drizzle_test_port_catalog.dat"
        self.port_file_delimiter = ':' # what we use to separate port:owner 
        self.debug = debug
        self.logging = system_manager.logging
        self.system_manager = system_manager
        if self.debug:
            self.logging.debug_class(self)
        

    def get_port_block(self, requester, base_port, block_size):
        """ Try to return a block of ports of size
            block_size, starting with base_port

            We take a target port and increment it
            until we find an unused port.  We make
            no guarantee of continuous ports, only
            that we will try to return block_size
            ports for use
    
            We can probably get fancier / smarter in the future
            but this should work for now :-/

        """
        assigned_ports = []
        current_port = base_port
        while len(assigned_ports) != block_size:
            new_port = (self.get_port(requester, current_port))
            assigned_ports.append(new_port)
            current_port = new_port+1
        return assigned_ports

    def get_port(self, requester, desired_port):
        """ Try to lock in the desired_port
            if not, we increment the value until
            we find an unused port.
            We take max / min port values from test-run.pl
            This is a bit bobo, but will work for now...

        """
        
        searching_for_port = 1
        attempts_remain = 100
        max_port_value = 32767
        min_port_value = 5001
        while searching_for_port and attempts_remain:
            # Check if the port is used
            if self.check_port_status(desired_port): 
                # assign it
                self.assign_port(requester, desired_port)
                return desired_port
            else: # increment the port and try again
                desired_port = desired_port + 1
                if desired_port >= max_port_value:
                    desired_port = min_port_value
                attempts_remain = attempts_remain - 1
        self.logging.error("Failed to assign a port in %d attempts")
        sys.exit(1)

    def check_port_status(self, port):
        """ Check if a port is in use, via the catalog file 
            which all copies of test-run.py should use

            Not *really* sure how well this works with multiple
            test-run.py instances...we'll see if we even need it 
            to work 

        """
        # read the catalog file
        port_catalog = self.process_port_catalog()
        if port not in port_catalog and not self.is_port_used(port):
            return 1
        else:
            return 0

    def is_port_used(self, port):
        """ See if a given port is used on the system """
        retcode, output = self.system_manager.execute_cmd("netstat -lant")
        # parse our output
        entry_list = output.split("\n")
        good_data = 0
        for entry in entry_list:
            if entry.startswith('Proto'):
                good_data = 1
            elif good_data:
                used_port = int(entry.split()[3].split(':')[-1].strip())
                if port == used_port:
                    if entry.split()[-1] != "TIME_WAIT":
                        return 1
        return 0

    def process_port_catalog(self):
        """ Read in the catalog file so that we can see
            if the port is in use or not

        """
        port_catalog = {}
        delimiter = ':'
        if os.path.exists(self.port_catalog):
            try:
                port_file = open(self.port_catalog,'r')
                for line in port_file:
                    line = line.strip()
                    port, owner = line.split(self.port_file_delimiter)
                    port_catalog[port] = owner
                port_file.close()
            except IOError, e:
                self.logging.error("Problem opening port catalog file: %s" %(self.port_catalog))
                self.logging.error("%s" %e)
                sys.exit(1)
        return port_catalog

    def assign_port(self, owner, port):
        """Assigns a port - logs it in the port_catalog file"""

        data_string = "%d:%s\n" %(port, owner)
        try:
            port_file = open(self.port_catalog,'a')
            port_file.write(data_string)
            port_file.close()
        except IOError, e:
            self.logging.error("Problem opening port catalog file: %s" %(self.port_catalog))
            self.logging.error("%s" %e)
            sys.exit(1)

    def free_ports(self, portlist):
       """ Clean up our port catalog """
       for port in portlist:
          self.free_port(port)

    def free_port(self, port):
       """ Free a single port from the catalog """
       if self.debug:
           self.logging.debug("Freeing port %d" %(port))
       port_catalog = self.process_port_catalog()
       port_catalog.pop(str(port),None)
       self.write_port_catalog(port_catalog)

    def write_port_catalog(self, port_catalog):
        port_file = open(self.port_catalog, 'w')
        for key, value in port_catalog.items():
            port_file.write(("%s:%s\n" %(key, value)))
        port_file.close()

        
       
       

