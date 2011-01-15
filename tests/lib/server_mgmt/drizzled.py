#! /usr/bin/python
# -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010 Patrick Crews
#

""" drizzled.py:  code to allow a serverManager
    to provision and start up a drizzled server object
    for test execution

"""

# imports
import os
import time

class drizzleServer():
    """ represents a drizzle server, its possessions
        (datadir, ports, etc), and methods for controlling
        and querying it

    """

    def __init__(self, name, server_manager, server_options
                , requester, workdir_root):
        self.skip_keys = [ 'server_manager'
                         , 'system_manager'
                         , 'dirset'
                         , 'preferred_base_port'
                         , 'no_secure_file_priv'
                         , 'secure_file_string'
                         , 'port_block'
                         ]
        self.debug = server_manager.debug
        self.verbose = server_manager.verbose
        self.initial_run = 1
        self.owner = requester
        self.server_options = server_options
        self.server_manager = server_manager
        self.system_manager = self.server_manager.system_manager
        self.logging = self.system_manager.logging
        self.no_secure_file_priv = self.server_manager.no_secure_file_priv
        self.code_tree = self.system_manager.code_tree
        self.preferred_base_port = 9306
        self.name = name
        self.status = 0 # stopped, 1 = running
        self.bad_run = 0 # was the last test a failure?  our state is suspect
        self.server_start_timeout = 60

        # Get our ports
        self.port_block = self.system_manager.port_manager.get_port_block( self.name
                                                                         , self.preferred_base_port
                                                                         , 4 )
        self.master_port = self.port_block[0]
        self.drizzle_tcp_port = self.port_block[1]
        self.mc_port = self.port_block[2]
        self.pbms_port = self.port_block[3]
        

        # Generate our working directories
        self.dirset = { self.name : { 'var': {'std_data_ln':( os.path.join(self.code_tree.testdir,'std_data'))
                                             ,'log':None
                                             ,'run':None
                                             ,'tmp':None
                                             ,'master-data': {'local': { 'test':None
                                                                       , 'mysql':None
                                                                       }
                                                             }
                                             }  
                                    } 
                      }
        self.workdir = self.system_manager.create_dirset( workdir_root
                                                        , self.dirset)
        self.vardir = os.path.join(self.workdir,'var')
        self.tmpdir = os.path.join(self.vardir,'tmp')
        self.rundir = os.path.join(self.vardir,'run')
        self.logdir = os.path.join(self.vardir,'log')
        self.datadir = os.path.join(self.vardir,'master-data')

        self.error_log = os.path.join(self.logdir,('%s.err' %(self.name)))
        self.pid_file = os.path.join(self.rundir,('%s.pid' %(self.name)))
        self.socket_file = os.path.join(self.vardir, ('%s.sock' %(self.name)))
        self.timer_file = os.path.join(self.logdir,('timer'))
        self.snapshot_path = os.path.join(self.tmpdir,('snapshot_%s' %(self.master_port)))
        # We want to use --secure-file-priv = $vardir by default
        # but there are times / tools when we need to shut this off
        if self.no_secure_file_priv:
            self.secure_file_string = ''
        else:
            self.secure_file_string = "--secure-file-priv='%s'" %(self.vardir)
        self.user_string = '--user=root'

        # client files
        self.drizzledump = self.code_tree.drizzledump
        self.drizzle_client = self.code_tree.drizzle_client
        self.drizzleimport = self.code_tree.drizzleimport
        self.drizzleadmin = self.code_tree.drizzleadmin
        self.drizzleslap = self.code_tree.drizzleslap
        self.server_path = self.code_tree.drizzle_server
        self.drizzle_client_path = self.code_tree.drizzle_client
        self.schemawriter = self.code_tree.schemawriter

        self.initialize_databases()
        self.take_db_snapshot()

        if self.debug:
            self.logging.debug_class(self)

    def report(self):
        """ We print out some general useful info """
        report_values = [ 'name'
                        , 'master_port'
                        , 'drizzle_tcp_port'
                        , 'mc_port'
                        , 'pbms_port'
                        , 'vardir'
                        , 'status'
                        ]
        self.logging.info("%s master server:" %(self.owner))
        for key in report_values:
          value = vars(self)[key] 
          self.logging.info("%s: %s" %(key.upper(), value))

    def start(self, expect_fail):
        """ Start the server, using the options in option_list
            as well as self.standard_options
            
            if expect_fail = 1, we know the server shouldn't 
            start up

        """
        if self.verbose:
            self.logging.verbose("Starting server: %s.%s" %(self.owner, self.name))
        start_cmd = "%s %s --mysql-protocol.port=%d --mysql-protocol.connect-timeout=60 --mysql-unix-socket-protocol.path=%s --pid-file=%s --drizzle-protocol.port=%d --datadir=%s --tmpdir=%s --innodb.data-file-path=ibdata1:20M:autoextend %s %s > %s 2>&1 & " % ( self.server_path
                                               , self.process_server_options()
                                               , self.master_port
                                               , self.socket_file
                                               , self.pid_file
                                               , self.drizzle_tcp_port
                                               , self.datadir
                                               , self.tmpdir
                                               , self.secure_file_string
                                               , self.user_string 
                                               , self.error_log
                                               )
        if self.debug:
            self.logging.debug("Starting server with:")
            self.logging.debug("%s" %(start_cmd))
        server_retcode = os.system(start_cmd)
        
        timer = 0
        timeout = self.server_start_timeout
        while not self.ping(quiet= True) and timer != timeout:
            time.sleep(1)
            timer= timer + 1
            # We make sure the server is running and return False if not 
            if timer == timeout and not self.ping(quiet= True):
                self.logging.error(( "Server failed to start within %d seconds.  This could be a problem with the test machine or the server itself" %(timeout)))
                retcode = 1
     
        if server_retcode == 0:
            self.status = 1 # we are running

        if server_retcode != 0 and not expect_fail:
            self.logging.error("Server startup command: %s failed with error code %d" %( start_cmd
                                                                                  , server_retcode))
        elif server_retcode == 0 and expect_fail:
            self.logging.error("Server startup command :%s expected to fail, but succeeded" %(start_cmd))
        return server_retcode and expect_fail

    def stop(self):
        """ Stop the server """
        if self.verbose:
            self.logging.verbose("Stopping server %s.%s" %(self.owner, self.name))
        stop_cmd = "%s --user=root --port=%d --shutdown " %(self.drizzle_client_path, self.master_port)
        if self.debug:
            self.logging.debug("%s" %(stop_cmd))
        retcode, output = self.system_manager.execute_cmd(stop_cmd)
        if retcode:
            self.logging.error("Problem shutting down server:")
            self.logging.error("%s : %s" %(retcode, output))
        else:
            self.status = 0 # indicate we are shutdown
           

    def ping(self, quiet= False):
        """Pings the server. Returns True if server is up and running, False otherwise."""
        ping_cmd= "%s --ping --port=%d" % (self.drizzle_client_path, self.master_port)
        if not quiet:
            self.logging.info("Pinging Drizzled server on port %d" % self.master_port)
        (retcode, output)= self.system_manager.execute_cmd(ping_cmd, must_pass = 0)
        return retcode == 0

    def process_server_options(self):
        """Consume the list of options we have been passed.
           Return a string with them joined

        """
        
        return " ".join(self.server_options)
                  
    def initialize_databases(self):
        """ Call schemawriter to make db.opt files """
        databases = [ 'test'
                    , 'mysql'
                    ]
        for database in databases:
            db_path = os.path.join(self.datadir,'local',database,'db.opt')
            cmd = "%s %s %s" %(self.schemawriter, database, db_path)
            self.system_manager.execute_cmd(cmd)

    def take_db_snapshot(self):
        """ Take a snapshot of our vardir for quick restores """
       
        self.logging.info("Taking clean db snapshot...")
        if os.path.exists(self.snapshot_path):
            # We need to remove an existing path as python shutil
            # doesn't want an existing target
            self.system_manager.remove_dir(self.snapshot_path)
        self.system_manager.copy_dir(self.datadir, self.snapshot_path)

    def restore_snapshot(self):
        """ Restore from a stored snapshot """
        
        if self.verbose:
            self.logging.verbose("Restoring from db snapshot")
        if not os.path.exists(self.snapshot_path):
            self.logging.error("Could not find snapshot: %s" %(self.snapshot_path))
        self.system_manager.remove_dir(self.datadir)
        self.system_manager.copy_dir(self.snapshot_path, self.datadir)

    def cleanup(self):
        """ Cleanup - just free ports for now..."""
        self.system_manager.port_manager.free_ports(self.port_block)

    def set_server_options(self, server_options):
        """ We update our server_options to the new set """
        self.server_options = server_options
         




