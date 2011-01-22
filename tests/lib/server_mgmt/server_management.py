#! /usr/bin/python
# -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010 Patrick Crews
#
"""server_management.py
   code for dealing with apportioning servers
   to suit the needs of the tests and executors

"""
# imports
import thread

class serverManager:
    """ code that handles the server objects
        We use this to track, do mass actions, etc
        Single point of contact for this business

    """

    def __init__(self, system_manager, variables):
        self.skip_keys = [ 'ld_preload'
                         , 'system_manager'
                         ]
        self.debug = variables['debug']
        self.verbose = variables['verbose']
        self.initial_run = 1
        self.server_base_name = 'server'
        self.no_secure_file_priv = variables['nosecurefilepriv']
        self.system_manager = system_manager
        self.logging = system_manager.logging
        self.code_tree = system_manager.code_tree
        self.servers = {}
        # We track this
        self.ld_preload = system_manager.ld_preload
        self.mutex = thread.allocate_lock()

        if self.debug:
            self.logging.debug_class(self)

    def request_servers( self, requester, workdir, master_count
                   , slave_count, server_options
                   , expect_fail = 0):
        """ We produce the server objects / start the server processes
            as requested.  We report errors and whatnot if we can't
            That is, unless we expect the server to not start, then
            we just return a value / message
    
        """

        # Make sure our server is in a decent state, if the last test
        # failed, then we reset the server
        self.check_server_status(requester)
        
        # Make sure we have the proper number of servers for this requester
        self.process_server_count( requester, master_count+slave_count
                                 , workdir, server_options)

        # Make sure we are running with the correct options 
        self.evaluate_existing_servers(requester, server_options)

        # Fire our servers up
        bad_start = self.start_servers(requester, expect_fail)
 
        # Return them to the requester
        return (self.get_server_list(requester), bad_start)        


    
    def allocate_server(self, requester, server_options, workdir):
        """ Intialize an appropriate server object.
            Start up occurs elsewhere

        """
        # Get a name for our server
        server_name = self.get_server_name(requester)

        # initialize a server_object
        if self.code_tree.type == 'Drizzle':
          from lib.server_mgmt.drizzled import drizzleServer as server_type
        new_server = server_type( server_name, self, server_options
                                , requester, workdir )
        self.log_server(new_server, requester)

    def start_servers(self, requester, expect_fail):
        """ Start all servers for the requester """
        bad_start = 0
        for server in self.get_server_list(requester):
            if server.status == 0:
                bad_start = bad_start + self.start_server( server
                                                         , requester
                                                         , expect_fail)
        return bad_start

    def start_server(self, server, requester, expect_fail):
        """ Start an individual server and return
            an error code if it did not start in a timely manner
 
        """
        bad_start = server.start(expect_fail)
        if bad_start:
            # Our server didn't start, we need to return an 
            # error
            return 1
        else:
            return 0
             

    def stop_server(self, server):
        """ Stop an individual server if it is running """
        if server.status == 1:
            server.stop()

    def stop_servers(self, requester):
        """ Stop all servers running for the requester """
        for server in self.get_server_list(requester):
            self.stop_server(server)

    def stop_server_list(self, server_list):
        """ Stop the servers in an arbitrary list of them """
        for server in server_list:
            self.stop_server(server)

    def stop_all_servers(self):
        """ Stop all running servers """

        self.logging.info("Stopping all running servers...")
        for server_list in self.servers.values():
            for server in server_list:
                self.stop_server(server)

    def cleanup_all_servers(self):
        """Mainly for freeing server ports for now """
        for server_list in self.servers.values():
            for server in server_list:
                server.cleanup()

    def cleanup(self):
        """Stop all servers and free their ports and whatnot """
        self.stop_all_servers()
        self.cleanup_all_servers()

    def get_server_name(self, requester):
        """ We name our servers requester.server_basename.count
            where count is on a per-requester basis
            We see how many servers this requester has and name things 
            appropriately

        """

        server_count = self.server_count(requester)
        return "%s%d" %(self.server_base_name, server_count)

    def has_servers(self, requester):
        """ Check if the given requester has any servers """
        if requester not in self.servers: # new requester
            self.servers[requester] = []
        return self.server_count(requester)

    def log_server(self, new_server, requester):
        self.servers[requester].append(new_server)

    def evaluate_existing_servers( self, requester, server_options):
        """ See if the requester has any servers and if they
            are suitable for the current test

        """
        current_servers = self.servers[requester]
        
        for server in current_servers:
            if self.compare_options( server.server_options
                                   , server_options):
                return 1
            else:
                # We need to reset what is running and change the server
                # options
                server_options = self.filter_server_options(server_options)
                self.reset_server(server)
                self.update_server_options(server, server_options)

    def filter_server_options(self, server_options):
        """ Remove a list of options we don't want passed to the server
            these are test-case specific options.
 
            NOTE: It is a bad hack to allow test-runner commands
            to mix with server options willy-nilly in master-opt files
            as we do.  We need to kill this at some point : (

        """
        remove_options = [ '--restart'
                         , '--skip-stack-trace'
                         , '--skip-core-file'
                         ]
        for remove_option in remove_options:
            if remove_option in server_options:
                server_options.remove(remove_option)
        return server_options
            
    
    def compare_options(self, optlist1, optlist2):
        """ Compare two sets of server options and see if they match """
        return sorted(optlist1) == sorted(optlist2)

    def reset_server(self, server):
        self.stop_server(server)
        server.restore_snapshot()
        server.reset()

    def reset_servers(self, requester):
        for server in self.servers[requester]:
            self.reset_server(server)


    def process_server_count(self, requester, desired_count, workdir, server_options):
        """ We see how many servers we have.  We shrink / grow
            the requesters set of servers as needed.

            If we shrink, we shutdown / reset the discarded servers
            (naturally)
 
        """
        server_options = self.filter_server_options(server_options)
        current_count = self.has_servers(requester)
        if desired_count > current_count:
            for i in range(desired_count - current_count):
                self.allocate_server(requester, server_options, workdir)
        elif desired_count < current_count:
            good_servers = self.get_server_list(requester)[:desired_count]
            retired_servers = self.get_server_list(requester)[desired_count - current_count:]
            self.stop_server_list(retired_servers)
            self.set_server_list(requester, good_servers)
            
         

    def server_count(self, requester):
        """ Return how many servers the the requester has """
        return len(self.servers[requester])

    def get_server_list(self, requester):
        """ Return the list of servers assigned to the requester """
        self.has_servers(requester) # initialize, hacky : (
        return self.servers[requester]
 
    def set_server_list(self, requester, server_list):
        """ Set the requesters list of servers to server_list """

        self.servers[requester] = server_list

    def add_server(self, requester, new_server):
       """ Add new_server to the requester's set of servers """
       self.servers[requester].append(new_server)

    def update_server_options(self, server, server_options):
        """ Change the option_list a server has to use on startup """
        if self.debug:
            self.logging.debug("Updating server: %s options" %(server.name))
            self.logging.debug("FROM: %s" %(server.server_options))
            self.logging.debug("TO: %s" %(server_options))
        server.set_server_options(server_options)

    def get_server_count(self):
        """ Find out how many servers we have out """
        server_count = 0
        for server_list in self.servers.values():
            for server in server_list:
                server_count = server_count + 1
        return server_count

    def check_server_status(self, requester):
        """ Make sure our servers are good,
            reset the otherwise.

        """
        for server in self.get_server_list(requester):
            if server.failed_test:
                self.reset_server(server)





        








