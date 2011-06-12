#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
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

"""codeTree

   definition of what a code tree should look like
   to the test-runner (which files /directories it should find where)
   
   Files paths can be in one of several locations that we locate via
   systemManager methods

"""
# imports
import os


class codeTree:
    """ Defines what files / directories we should find and where
        allows for optional / required.

    """
  
    def __init__(self, variables, system_manager):
        self.debug = variables['debug']
        self.system_manager = system_manager
        self.logging = system_manager.logging

    def debug_status(self):
            self.logging.debug(self)
            for key, item in sorted(vars(self).items()):
                self.logging.debug("%s: %s" %(key, item))

class drizzleTree(codeTree):
    """ What a Drizzle code tree should look like to the test-runner
    
    """

    def __init__(self, variables,system_manager):
        self.system_manager = system_manager
        self.logging = self.system_manager.logging
        self.skip_keys = ['ld_lib_paths']
        self.debug = variables['debug']
        self.verbose = variables['verbose']
        self.basedir = self.system_manager.find_path([os.path.abspath(variables['basedir'])])
        self.source_dist = os.path.isdir(os.path.join(self.basedir, 'drizzled'))
        self.builddir = self.system_manager.find_path([os.path.abspath(self.basedir)])
        self.top_builddir = variables['topbuilddir']
        self.testdir = self.system_manager.find_path([os.path.abspath(variables['testdir'])])
        self.clientbindir = self.system_manager.find_path([os.path.join(self.builddir, 'client')
                                     , os.path.join(self.basedir, 'client')
                                     , os.path.join(self.basedir, 'bin')])
        self.srcdir = self.system_manager.find_path([self.basedir])
        self.suite_paths = [ os.path.join(self.basedir,'plugin')
                           , os.path.join(self.testdir,'suite')
                           ]


        self.drizzle_client = self.system_manager.find_path([os.path.join(self.clientbindir,
                                                     'drizzle')])

        self.drizzledump = self.system_manager.find_path([os.path.join(self.clientbindir,
                                                     'drizzledump')])

        self.drizzleimport = self.system_manager.find_path([os.path.join(self.clientbindir,
                                                     'drizzleimport')])

        self.drizzle_server = self.system_manager.find_path([os.path.join(self.basedir,'drizzled/drizzled'),
                                         os.path.join(self.clientbindir,'drizzled'),
                                         os.path.join(self.basedir,'libexec/drizzled'),
                                         os.path.join(self.basedir,'bin/drizzled'),
                                         os.path.join(self.basedir,'sbin/drizzled'),
                                         os.path.join(self.builddir,'drizzled/drizzled')])


        self.drizzleslap = self.system_manager.find_path([os.path.join(self.clientbindir,
                                                     'drizzleslap')])

        self.schemawriter = self.system_manager.find_path([os.path.join(self.basedir,
                                                     'drizzled/message/schema_writer'),
                                        os.path.join(self.builddir,
                                                     'drizzled/message/schema_writer')])

        self.drizzletest = self.system_manager.find_path([os.path.join(self.clientbindir,
                                                   'drizzletest')])

        self.drizzle_trx_reader = self.system_manager.find_path([os.path.join(self.basedir,
                                                                 'plugin/transaction_log/utilities/drizzletrx')])

        self.server_version_string = None
        self.server_executable = None
        self.server_version = None
        self.server_compile_os = None
        self.server_platform = None
        self.server_compile_comment = None
        self.type = 'Drizzle'

        self.process_server_version()
        self.ld_lib_paths = self.get_ld_lib_paths()
         
        self.report()

        if self.debug:
            self.logging.debug_class(self)

    def report(self):
        self.logging.info("Using Drizzle source tree:")
        report_keys = ['basedir'
                      ,'clientbindir'
                      ,'testdir'
                      ,'server_version'
                      ,'server_compile_os'
                      ,'server_platform'
                      ,'server_comment']
        for key in report_keys:
            self.logging.info("%s: %s" %(key, vars(self)[key]))
        


    def process_server_version(self):
        """ Get the server version number from the found server executable """
        (retcode, self.server_version_string) = self.system_manager.execute_cmd(("%s --no-defaults --version" %(self.drizzle_server)))
        # This is a bit bobo, but we're doing it, so nyah
        # TODO fix this : )
        self.server_executable, data_string = [data_item.strip() for data_item in self.server_version_string.split('Ver ')]
        self.server_version, data_string = [data_item.strip() for data_item in data_string.split('for ')]
        self.server_compile_os, data_string = [data_item.strip() for data_item in data_string.split(' on')]
        self.server_platform = data_string.split(' ')[0].strip()
        self.server_comment = data_string.replace(self.server_platform,'').strip()

    def get_ld_lib_paths(self):
        """ Return a list of paths we want added to LD_LIB variables

            These are processed later by the system manager, but we want to 
            specify them here (for a drizzle source tree) and now
  
        """
        ld_lib_paths = []
        if self.source_dist:
            ld_lib_paths = [ os.path.join(self.basedir,"libdrizzleclient/.libs/")
                           , os.path.join(self.basedir,"libdrizzle-2.0/libdrizzle.libs")
                           , os.path.join(self.basedir,"libdrizzle/.libs")
                           , os.path.join(self.basedir,"libdrizzle-2.0/libdrizzle/.libs")
                           , os.path.join(self.basedir,"libdrizzle-1.0/libdrizzle/.libs")
                           , os.path.join(self.basedir,"mysys/.libs/")
                           , os.path.join(self.basedir,"mystrings/.libs/")
                           , os.path.join(self.basedir,"drizzled/.libs/")
			                     , "/usr/local/lib"
                           ]
        else:
            ld_lib_paths = [ os.path.join(self.basedir,"lib")]
        return ld_lib_paths

        
