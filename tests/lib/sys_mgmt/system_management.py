#! /usr/bin/python
# -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010 Patrick Crews
#
"""system_management.py
   code for dealing with system-level 'stuff'.
   This includes setting environment variables, looking for clients,
   so on and so forth.

   These things are / should be constant regardless of the testing being done
   We do an initial preflight / setup, we then call the mode-specific
   system_initialise() to do whatever the testing mode requires to do that
   voodoo that it do so well

"""

# imports
import os
import sys
import shutil
import commands

from lib.sys_mgmt.port_management import portManager
from lib.sys_mgmt.logging_management import loggingManager
from lib.sys_mgmt.time_management import timeManager

class systemManager:
    """Class to deal with the basics of system-level interaction
       and awareness

       Uses other managers to handle sub-tasks like port management

    """
    def __init__(self, variables, tree_type='drizzle'):
        self.logging = loggingManager(variables)
        if variables['verbose']:
            self.logging.verbose("Initializing system manager...")

        self.skip_keys = [ 'code_tree'
                         , 'ld_lib_paths'
                         , 'port_manager'
                         , 'logging_manager'
                         , 'environment_reqs'
                         , 'env_var_delimiter']
        self.debug = variables['debug']
        self.verbose = variables['verbose']
        self.env_var_delimiter = ':'
        self.no_shm = variables['noshm']
        self.shm_path = self.find_path(["/dev/shm", "/tmp"], required=0)
        self.cur_os = os.uname()[0]
        self.symlink_name = 'dtr_work_sym'
        self.workdir = variables['workdir']
        self.start_dirty = variables['startdirty']
        self.valgrind = variables['valgrind']
        self.gdb = variables['gdb']

        # we use this to preface commands in order to run valgrind and such
        self.cmd_prefix = '' 
        
        self.port_manager = portManager(self,variables['debug'])
        self.time_manager = timeManager(self)
       
            
        # Make sure the tree we are testing looks good
        self.code_tree = self.get_code_tree(variables, tree_type)

        self.ld_lib_paths = self.code_tree.ld_lib_paths

        # Some ENV vars are system-standard
        # We describe and set them here and now
        # The format is name: (value, append, suffix)
        self.environment_reqs = { 'UMASK':('0660',0,0)
                                , 'UMASK_DIR' : ('0770',0,0)
                                , 'LC_ALL' : ('C',0,0)
                                , 'LC_CTYPE' : ('C',0,0)
                                , 'LC_COLLATE' : ('C',0,0)
                                , 'USE_RUNNING_SERVER' : ("0",0,0)
                                , 'TOP_SRCDIR' : (self.code_tree.srcdir,0,0)
                                , 'TOP_BUILDDIR' : (self.code_tree.builddir,0,0)
                                , 'DRIZZLE_TEST_DIR' : (self.code_tree.testdir,0,0)
                                , 'DTR_BUILD_THREAD' : ("-69.5",0,0)
                                , 'LD_LIBRARY_PATH' : (self.ld_lib_paths,1,1)
                                , 'DYLD_LIBRARY_PATH' : (self.ld_lib_paths,1,1)
                                }
        # set the env vars we need
        self.process_environment_reqs(self.environment_reqs)

        # initialize our workdir
        self.process_workdir()

        # check for libtool
        self.libtool = self.libtool_check()

        # do we need to setup for valgrind?
        if self.valgrind:
            self.handle_valgrind_reqs(variables['valgrindarglist'])
            

        if self.debug:
            self.logging.debug_class(self)
        


    def get_code_tree(self, variables, tree_type):
        """Find out the important files, directories, and env. vars
           for a particular type of tree.  We import a definition module
           depending on the tree_type.  The module lets us know
           what to look for, etc

        """
        
        # Import the appropriate module that defines
        # where we find what we need depending on 
        # tree type
        test_tree = self.process_tree_type(tree_type, variables)
        return test_tree

    

    def process_tree_type(self, tree_type, variables):
        """Import the appropriate module depending on the type of tree
           we are testing. 

           Drizzle is the only supported type currently

        """
        if self.verbose:
            self.logging.verbose("Processing source tree under test...")
        if tree_type == 'drizzle':
            # base_case
            from lib.sys_mgmt.codeTree import drizzleTree
            test_tree = drizzleTree(variables,self)
            return test_tree
        else:
            self.logging.error("Tree_type: %s not supported yet" %(tree_type))
            sys.exit(1)

   
    def process_environment_reqs(self, environment_reqs, quiet=0):
        """ We process a dictionary in a specific format
            that asks for various env vars to be set
            These values can be used to overwrite,
            append, or prepend a new value string to
            a named variable.

            Currently, we require multiple values to
            already to joined into a single string
            We can fix this later

        """
 
        for var_name, data_tuple in environment_reqs.items():
            value, append_flag, suffix_flag = data_tuple
            if type(value) is list:
                value = self.join_env_var_values(value)
            if append_flag:
                self.append_env_var(var_name, value, suffix_flag, quiet=quiet)
            else:
                self.set_env_var(var_name, value, quiet)
            
            

    def get_port_block(self, requester, base_port, block_size):
        """ Try to assign a block of ports for test execution
            purposes

        """
     
        return self.port_manager.get_port_block( requester
                                               , base_port, block_size)

    def create_dirset(self, rootdir, dirset):
        """ We produce the set of directories defined in dirset
            dirset is a set of dictionaries like
            {'dirname': 'subdir'}
            or {'dirname': {'subdir':'subsubdir}}...

            We generally expect there to be only a single
            top-level key.  The intent is to produce a dirset
            rooted at key[0], with various subdirs under that
            subsequest dirsets should be handles in separate calls...

        """
        for dirname in dirset.keys():
            full_path = os.path.join(rootdir, dirname)
            subdirset = dirset[dirname]
            if type(subdirset) is str:
                self.create_symlink(subdirset,full_path)
            else:
                self.create_dir(full_path)        
                # dirset[dirname] is a new dictionary
                if subdirset is None:
                    {}
                else:
                    self.create_dirset(full_path,subdirset)

        return full_path    

    def process_workdir(self):
        """ We create our workdir, analyze relevant variables
            to see if we should/shouldn't symlink to shm
            We do nothing if we have --start-dirty

        """
        
        if os.path.exists(self.workdir):
            # our workdir already exists
            if self.start_dirty:
                self.logging.info("Using --start-dirty, not attempting to touch directories")
                return
            else:
                self.remove_dir(self.workdir)
        self.allocate_workdir()
    

    def allocate_workdir(self):
        """ Create a workdir according to user-supplied specs """
        if self.no_shm:
            self.logging.info("Using --no-shm, will not link workdir to shm")
            self.create_dir(self.workdir, subdir=0)
        elif self.shm_path == None:
            self.logging.info("Could not find shared memory path for use.  Not linking workdir to shm")
            self.create_dir(self.workdir, subdir=0)
        else:
            shm_workdir = self.create_dir(os.path.join(self.shm_path, self.symlink_name))
            self.logging.info("Linking workdir %s to %s" %(self.workdir, shm_workdir))  
            self.create_symlink(shm_workdir, self.workdir)

    def create_dir(self, dirname, subdir =1 ):
        """ Create a directory.  If subdir = 1,
            then the new dir should be a subdir of
            self.workdir.  Else, we just create dirname,
            which should really be dirpath in this case

        """

        if subdir:
            full_path = os.path.join(self.workdir, dirname)
        else:
            full_path = dirname

        if os.path.exists(full_path):
            if self.start_dirty:
                return full_path
            else:
                shutil.rmtree(full_path)
            if self.debug:
                 self.logging.debug("Creating directory: %s" %(dirname))   
        os.makedirs(full_path)
        return full_path

    def remove_dir(self, dirname, require_empty=0 ):
        """ Remove the directory in question.
            We assume we want to brute-force clean
            things.  If require_empty = 0, then
            the dir must be empty to remove it

        """
        if self.debug:
            self.logging.debug("Removing directory: %s" %(dirname))
        if os.path.islink(dirname):
            os.remove(dirname)
        elif require_empty:
            os.rmdir(dirname)
        else:
            shutil.rmtree(dirname)

    def copy_dir(self, srcdir, tgtdir, overwrite = 1):
        """ Copy the contents of srcdir to tgtdir.
            We overwrite (remove/recreate) tgtdir
            if overwrite == 1

        """
        if self.debug:
            self.logging.debug("Copying directory: %s to %s" %(srcdir, tgtdir))
        if os.path.exists(tgtdir):
            if overwrite:
                self.remove_dir(tgtdir)
            else:
                self.logging.error("Cannot overwrite existing directory: %s" %(tgtdir))
                sys.exit(1)
        shutil.copytree(srcdir, tgtdir, symlinks=True)

    def create_symlink(self, source, link_name):
        """ We create a symlink to source named link_name """
        if self.debug:
            self.logging.debug("Creating symlink from %s to %s" %(source, link_name))
        if os.path.exists(link_name) or os.path.islink(link_name):
            os.remove(link_name)
        return os.symlink(source, link_name)

    def create_symlinks(self, needed_symlinks):
        """ We created the symlinks in needed_symlinks 
            We expect it to be tuples in source, link_name format

        """
        
        for needed_symlink in needed_symlinks:
            source, link_name = needed_symlink
            self.create_symlink(source, link_name)

    def join_env_var_values(self, value_list):
        """ Utility to join multiple values into a nice string
            for setting an env var to
 
        """

        return self.env_var_delimiter.join(value_list)

    def set_env_var(self, var_name, var_value, quiet=0):
        """Set an environment variable.  We really just abstract
           voodoo on os.environ

        """
        if self.debug and not quiet:
            self.logging.debug("Setting env var: %s" %(var_name))
        try:
            os.environ[var_name]=var_value
        except Exception, e:
            self.logging.error("Issue setting environment variable %s to value %s" %(var_name, var_value))
            self.logging.error("%s" %(e))
            sys.exit(1)

    def append_env_var(self, var_name, append_string, suffix=1, quiet=0):
        """ We add the values in var_values to the environment variable 
            var_name.  Depending on suffix value, we either append or prepend
            Use set_env_var to just overwrite an ENV var

        """
        new_var_value = ""
        if var_name in os.environ:
            cur_var_value = os.environ[var_name]
            if suffix: # We add new values to end of existing value
                new_var_values = [ cur_var_value, append_string ]
            else:
                new_var_values = [ append_string, cur_var_value ]
            new_var_value = self.env_var_delimiter.join(new_var_values)
        else:
            # No existing variable value
            new_var_value = append_string
        self.set_env_var(var_name, new_var_value, quiet=quiet)

    def find_path(self, paths, required=1):
        """We search for the files we need / want to be aware of
           such as the drizzled binary, the various client binaries, etc
           We use the required switch to determine if we die or not
           if we can't find the file.

           We expect paths to be a list of paths, ordered in terms
           of preference (ie we want to use something from search-path1
           before search-path2).

           We return None if no match found and this wasn't required

        """

        for test_path in paths:
            if self.debug:
                self.logging.debug("Searching for path: %s" %(test_path))
            if os.path.exists(test_path):
                return test_path
        if required:
            self.logging.error("Required file not found out of options: %s" %(" ,".join(paths)))
            sys.exit(1)
        else:
            return None

    def execute_cmd(self, cmd, must_pass = 1):
        """ Utility function to execute a command and
            return the output and retcode

        """

        if self.debug:
            self.logging.debug("Executing command: %s" %(cmd))
        (retcode, output)= commands.getstatusoutput(cmd)
        if not retcode == 0 and must_pass:
            self.logging.error("Command %s failed with retcode %d" %(cmd, retcode))
            self.logging.error("%s" %(output))
            sys.exit(1)
        return retcode, output

    def libtool_check(self):
        """ We search for libtool """
        libtool_path = '../libtool'
        if os.path.exists(libtool_path) and os.access( libtool_path
                                                     , os.X_OK):
            if self.valgrind or self.gdb:
                self.logging.info("Using libtool when running valgrind or debugger")
            return libtool_path
        else:
            return None

    def handle_valgrind_reqs(self, optional_args, mode='valgrind'):
        """ We do what voodoo we need to do to run valgrind """
        valgrind_args = [ "--show-reachable=yes"
                        , "--malloc-fill=0xDEADBEEF"
                        , "--free-fill=0xDEADBEEF"
                        # , "--trace-children=yes" this is for callgrind only
                        ]
        if optional_args:
        # we override the defaults with user-specified options
            valgrind_args = optional_args
        self.logging.info("Running valgrind with options: %s" %(" ".join(valgrind_args)))

        # set our environment variable
        self.set_env_var('VALGRIND_RUN', '1', quiet=0)

        # generate command prefix to call valgrind
        cmd_prefix = ''
        if self.libtool:
            cmd_prefix = "%s --mode=execute valgrind " %(self.libtool)
        if mode == 'valgrind':
            # default mode

            args = [ "--tool=memcheck"
                   , "--leak-check=yes"
                   , "--num-callers=16" 
                   ]
            # look for our suppressions file and add it to the mix if found
            suppress_file = os.path.join(self.code_tree.testdir,'valgrind.supp')
            if os.path.exists(suppress_file):
                args = args + [ "--suppressions=%s" %(suppress_file) ]

            cmd_prefix = cmd_prefix + " ".join(args + valgrind_args)
        self.cmd_prefix = cmd_prefix  
        
        # add debug libraries to ld_library_path
        debug_path = '/usr/lib/debug'
        if os.path.exists(debug_path):
            self.append_env_var("LD_LIBRARY_PATH", debug_path, suffix=1)
            self.append_env_var("DYLD_LIBRARY_PATH", debug_path, suffix=1)
    


 
        
        
        
