# -*- cperl -*-
# Copyright (C) 2006 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# This is a library file used by the Perl version of drizzle-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;
use File::Spec;

# These are not to be prefixed with "dtr_"

sub run_stress_test ();

##############################################################################
#
#  Run tests in the stress mode
#
##############################################################################

sub run_stress_test () 
{

  my $args;
  my $stress_suitedir;

  dtr_report("Starting stress testing\n");

  if ( ! $::glob_use_embedded_server )
  {
    if ( ! drizzled_start($::master->[0],[],[]) )
    {
      dtr_error("Can't start the drizzled server");
    }
  }

  my $stress_basedir=File::Spec->catdir($::opt_vardir, "stress");
  
  #Clean up stress dir 
  if ( -d $stress_basedir )
  {
    rmtree($stress_basedir);
  }
  mkpath($stress_basedir);
 
  if ($::opt_stress_suite ne 'main' && $::opt_stress_suite ne 'default' )
  {
    $stress_suitedir=File::Spec->catdir($::glob_drizzle_test_dir, "suite", 
                                         $::opt_stress_suite);
  }
  else
  {
    $stress_suitedir=$::glob_drizzle_test_dir;
  }

  if ( -d $stress_suitedir )
  {
    #$stress_suite_t_dir=File::Spec->catdir($stress_suitedir, "t");
    #$stress_suite_r_dir=File::Spec->catdir($stress_suitedir, "r");    
    #FIXME: check dirs above for existence to ensure that test suite 
    #       contains tests and results dirs
  }
  else
  {
    dtr_error("Specified test suite $::opt_stress_suite doesn't exist");
  }
 
  if ( @::opt_cases )
  {
    $::opt_stress_test_file=File::Spec->catfile($stress_basedir, "stress_tests.txt");
    open(STRESS_FILE, ">$::opt_stress_test_file");
    print STRESS_FILE join("\n",@::opt_cases),"\n";
    close(STRESS_FILE);
  }
  elsif ( $::opt_stress_test_file )
  {
    $::opt_stress_test_file=File::Spec->catfile($stress_suitedir, 
                                              $::opt_stress_test_file);
    if ( ! -f $::opt_stress_test_file )
    {
      dtr_error("Specified file $::opt_stress_test_file with list of tests does not exist\n",
                "Please ensure that file exists and has proper permissions");
    }
  }
  else
  {
    $::opt_stress_test_file=File::Spec->catfile($stress_suitedir, 
                                              "stress_tests.txt");
    if ( ! -f $::opt_stress_test_file )
    {
      dtr_error("Default file $::opt_stress_test_file with list of tests does not exist\n",
          "Please use --stress-test-file option to specify custom one or you can\n",
          "just specify name of test for testing as last argument in command line");

    }    
  }

  if ( $::opt_stress_init_file )
  {
    $::opt_stress_init_file=File::Spec->catfile($stress_suitedir, 
                                              $::opt_stress_init_file);
    if ( ! -f $::opt_stress_init_file )
    {
      dtr_error("Specified file $::opt_stress_init_file with list of tests does not exist\n",
                "Please ensure that file exists and has proper permissions");
    }
  }
  else
  {
    $::opt_stress_init_file=File::Spec->catfile($stress_suitedir, 
                                              "stress_init.txt");
    if ( ! -f $::opt_stress_init_file )
    {
      $::opt_stress_init_file='';
    }
  }  
  
  if ( $::opt_stress_mode ne 'random' && $::opt_stress_mode ne 'seq' )
  {
    dtr_error("You specified wrong mode $::opt_stress_mode for stress test\n",
              "Correct values are 'random' or 'seq'");
  }

  dtr_init_args(\$args);
  
  dtr_add_arg($args, "--server-socket=%s", $::master->[0]->{'path_sock'});
  dtr_add_arg($args, "--server-user=%s", $::opt_user);
  dtr_add_arg($args, "--server-database=%s", "test");  
  dtr_add_arg($args, "--stress-suite-basedir=%s", $::glob_drizzle_test_dir);  
  dtr_add_arg($args, "--suite=%s", $::opt_stress_suite);
  dtr_add_arg($args, "--stress-tests-file=%s", $::opt_stress_test_file);      
  dtr_add_arg($args, "--stress-basedir=%s", $stress_basedir);
  dtr_add_arg($args, "--server-logs-dir=%s", $stress_basedir);
  dtr_add_arg($args, "--stress-mode=%s", $::opt_stress_mode);
  dtr_add_arg($args, "--drizzletest=%s", $::exe_drizzletest);
  dtr_add_arg($args, "--threads=%s", $::opt_stress_threads);
  dtr_add_arg($args, "--verbose");
  dtr_add_arg($args, "--cleanup");
  dtr_add_arg($args, "--log-error-details");
  dtr_add_arg($args, "--abort-on-error");

  if ( $::opt_stress_init_file )
  {
    dtr_add_arg($args, "--stress-init-file=%s", $::opt_stress_init_file);
  }

  if ( !$::opt_stress_loop_count && !$::opt_stress_test_count &&
       !$::opt_stress_test_duration )
  {
    #Limit stress testing with 20 loops in case when any limit parameter 
    #was specified 
    $::opt_stress_test_count=20;
  }

  if ( $::opt_stress_loop_count )
  {
    dtr_add_arg($args, "--loop-count=%s", $::opt_stress_loop_count);
  }

  if ( $::opt_stress_test_count )
  {
    dtr_add_arg($args, "--test-count=%s", $::opt_stress_test_count);
  }

  if ( $::opt_stress_test_duration )
  {
    dtr_add_arg($args, "--test-duration=%s", $::opt_stress_test_duration);
  }

  #Run stress test
  dtr_run("$::glob_drizzle_test_dir/drizzle-stress-test.pl", $args, "", "", "", "");
  if ( ! $::glob_use_embedded_server )
  {
    stop_all_servers();
  }
}

1;
