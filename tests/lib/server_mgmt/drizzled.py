#! /usr/bin/env python
# -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010 Patrick Crews
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


""" drizzled.py:  code to allow a serverManager
    to provision and start up a drizzled server object
    for test execution

"""

# imports
import os

class drizzleServer():
    """ represents a drizzle server, its possessions
        (datadir, ports, etc), and methods for controlling
        and querying it

        TODO: create a base server class that contains
              standard methods from which we can inherit
              Currently there are definitely methods / attr
              which are general

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
        # We register with server_manager asap
        self.server_manager.log_server(self, requester)

        self.system_manager = self.server_manager.system_manager
        self.valgrind = self.system_manager.valgrind
        self.gdb = self.system_manager.gdb
        if self.valgrind:
            self.valgrind_time_buffer = 10
        else:
            self.valgrind_time_buffer = 1
        self.cmd_prefix = self.system_manager.cmd_prefix
        self.logging = self.system_manager.logging
        self.no_secure_file_priv = self.server_manager.no_secure_file_priv
        self.code_tree = self.system_manager.code_tree
        self.preferred_base_port = 9306
        self.name = name
        self.status = 0 # stopped, 1 = running
        self.tried_start = 0
        self.failed_test = 0 # was the last test a failure?  our state is suspect
        self.server_start_timeout = 60 * self.valgrind_time_buffer

        # Get our ports
        self.port_block = self.system_manager.port_manager.get_port_block( self.name
                                                                         , self.preferred_base_port
                                                                         , 5 )
        self.master_port = self.port_block[0]
        self.drizzle_tcp_port = self.port_block[1]
        self.mc_port = self.port_block[2]
        self.pbms_port = self.port_block[3]
        self.rabbitmq_node_port = self.port_block[4]
        

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
                        , 'rabbitmq_node_port'
                        , 'vardir'
                        , 'status'
                        ]
        self.logging.info("%s master server:" %(self.owner))
        for key in report_values:
          value = vars(self)[key] 
          self.logging.info("%s: %s" %(key.upper(), value))

    def get_start_cmd(self):
        """ Return the command string that will start up the server 
            as desired / intended
 
        """

        server_args = [ self.process_server_options()
                      , "--mysql-protocol.port=%d" %(self.master_port)
                      , "--mysql-protocol.connect-timeout=60"
                      , "--innodb.data-file-path=ibdata1:20M:autoextend"
                      , "--sort-buffer-size=256K"
                      , "--max-heap-table-size=1M"
                      , "--mysql-unix-socket-protocol.path=%s" %(self.socket_file)
                      , "--pid-file=%s" %(self.pid_file)
                      , "--drizzle-protocol.port=%d" %(self.drizzle_tcp_port)
                      , "--datadir=%s" %(self.datadir)
                      , "--tmpdir=%s" %(self.tmpdir)
                      , self.secure_file_string
                      , self.user_string
                      ]

        if self.gdb:
            server_args.append('--gdb')
            return self.system_manager.handle_gdb_reqs(self, server_args)
        else:
            return "%s %s %s & " % ( self.cmd_prefix
                                   , self.server_path
                                   , " ".join(server_args)
                                   )


    def get_stop_cmd(self):
        """ Return the command that will shut us down """
        
        return "%s --user=root --port=%d --shutdown " %(self.drizzle_client_path, self.master_port)
           

    def get_ping_cmd(self):
        """Return the command string that will 
           ping / check if the server is alive 

        """

        return "%s --ping --port=%d --user=root" % (self.drizzle_client_path, self.master_port)

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

    def reset(self):
        """ Voodoo to reset ourselves """
        self.failed_test = 0
         




