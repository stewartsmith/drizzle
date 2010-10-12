# -*- cperl -*-
# Copyright (C) 2004-2006 MySQL AB
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
use warnings;

sub dtr_report_test_name($);
sub dtr_report_test_passed($);
sub dtr_report_test_failed($);
sub dtr_report_test_skipped($);
sub dtr_report_test_not_skipped_though_disabled($);

sub dtr_report_stats ($);
sub dtr_print_line ();
sub dtr_print_thick_line ();
sub dtr_print_header ();
sub dtr_report (@);
sub dtr_warning (@);
sub dtr_error (@);
sub dtr_child_error (@);
sub dtr_debug (@);
sub dtr_verbose (@);

my $tot_real_time= 0;



##############################################################################
#
#  
#
##############################################################################

sub dtr_report_test_name ($) {
  my $tinfo= shift;
  my $tname= $tinfo->{name};

  $tname.= " '$tinfo->{combination}'"
    if defined $tinfo->{combination};

  _dtr_log($tname);
  if ($::opt_subunit) {
    printf "test: $tname\n";
  } else {
    printf "%-60s ", $tname;
  }
}

sub dtr_report_test_skipped ($) {
  my $tinfo= shift;
  my $tname= $tinfo->{name};
  my $cause= "";

  $tinfo->{'result'}= 'DTR_RES_SKIPPED';
  if ( $tinfo->{'disable'} )
  {
    $cause.= "disable";
  }
  else
  {
    $cause.= "skipped";
  }
  if ( $tinfo->{'comment'} )
  {
    if ($::opt_subunit) {
      dtr_report("skip: $tname [\ncause: $cause\n$tinfo->{'comment'}\n]");
    } else { 
      dtr_report("[ $cause ]   $tinfo->{'comment'}");
    }
  }
  else
  {
    if ($::opt_subunit) {
      dtr_report("skip: $tname");
    } else {
      dtr_report("[ $cause ]");
    }
  }
}

sub dtr_report_tests_not_skipped_though_disabled ($) {
  my $tests= shift;

  if ( $::opt_enable_disabled )
  {
    my @disabled_tests= grep {$_->{'dont_skip_though_disabled'}} @$tests;
    if ( @disabled_tests )
    {
      print "\nTest(s) which will be run though they are marked as disabled:\n";
      foreach my $tinfo ( sort {$a->{'name'} cmp $b->{'name'}} @disabled_tests )
      {
        printf "  %-20s : %s\n", $tinfo->{'name'}, $tinfo->{'comment'};
      }
    }
  }
}

sub dtr_report_test_passed ($) {
  my $tinfo= shift;
  my $tname= $tinfo->{name};

  my $timer=  "";
  if ( $::opt_timer and -f "$::opt_vardir/log/timer" )
  {
    $timer= dtr_fromfile("$::opt_vardir/log/timer");
    $tot_real_time += ($timer/1000);
    $timer= sprintf "%7s", $timer;
    ### XXX: How to format this as iso6801 datetime?
  }
  $tinfo->{'result'}= 'DTR_RES_PASSED';
  if ($::opt_subunit) {
    dtr_report("success: $tname");
  } else {
    dtr_report("[ pass ] $timer");
  }
}

sub dtr_report_test_failed ($) {
  my $tinfo= shift;
  my $tname= $tinfo->{name};
  my $comment= "";

  $tinfo->{'result'}= 'DTR_RES_FAILED';
  if ( defined $tinfo->{'timeout'} )
  {
    $comment.= "timeout";
  }
  elsif ( $tinfo->{'comment'} )
  {
    # The test failure has been detected by drizzle-test-run.pl
    # when starting the servers or due to other error, the reason for
    # failing the test is saved in "comment"
    $comment.= "$tinfo->{'comment'}";
  }
  elsif ( -f $::path_timefile )
  {
    # Test failure was detected by test tool and it's report
    # about what failed has been saved to file. Display the report.
    $comment.= dtr_fromfile($::path_timefile);
  }
  else
  {
    # Neither this script or the test tool has recorded info
    # about why the test has failed. Should be debugged.
    $comment.= "Unexpected termination, probably when starting drizzled";
  }
  if ($::opt_subunit) {
    dtr_report("failure: $tname [\n$comment\n]");
  } else {
    dtr_report("[ fail ]\n$comment");
  }
}

sub dtr_report_stats ($) {
  my $tests= shift;

  # ----------------------------------------------------------------------
  # Find out how we where doing
  # ----------------------------------------------------------------------

  my $tot_skiped= 0;
  my $tot_passed= 0;
  my $tot_failed= 0;
  my $tot_tests=  0;
  my $tot_restarts= 0;
  my $found_problems= 0; # Some warnings in the logfiles are errors...

  foreach my $tinfo (@$tests)
  {
    if ( $tinfo->{'result'} eq 'DTR_RES_SKIPPED' )
    {
      $tot_skiped++;
    }
    elsif ( $tinfo->{'result'} eq 'DTR_RES_PASSED' )
    {
      $tot_tests++;
      $tot_passed++;
    }
    elsif ( $tinfo->{'result'} eq 'DTR_RES_FAILED' )
    {
      $tot_tests++;
      $tot_failed++;
    }
    if ( $tinfo->{'restarted'} )
    {
      $tot_restarts++;
    }
  }

  # ----------------------------------------------------------------------
  # Print out a summary report to screen
  # ----------------------------------------------------------------------

  if ( ! $tot_failed )
  {
    print "All $tot_tests tests were successful.\n";
  }
  else
  {
    my $ratio=  $tot_passed * 100 / $tot_tests;
    print "Failed $tot_failed/$tot_tests tests, ";
    printf("%.2f", $ratio);
    print "\% were successful.\n\n";
    print
      "The log files in var/log may give you some hint\n",
      "of what went wrong.\n",
      "If you want to report this error, go to:\n",
      "\thttp://bugs.launchpad.net/drizzle\n";
  }
  if (!$::opt_extern)
  {
    print "The servers were restarted $tot_restarts times\n";
  }

  if ( $::opt_timer )
  {
    use English;

    dtr_report("Spent", sprintf("%.3f", $tot_real_time),"of",
	       time - $BASETIME, "seconds executing testcases");
  }

  print "\n";

  # Print a list of testcases that failed
  if ( $tot_failed != 0 )
  {
    my $test_mode= join(" ", @::glob_test_mode) || "default";
    print "drizzle-test-run in $test_mode mode: *** Failing the test(s):";

    foreach my $tinfo (@$tests)
    {
      if ( $tinfo->{'result'} eq 'DTR_RES_FAILED' )
      {
        print " $tinfo->{'name'}";
      }
    }
    print "\n";

  }

  # Print a list of check_testcases that failed(if any)
  if ( $::opt_check_testcases )
  {
    my @check_testcases= ();

    foreach my $tinfo (@$tests)
    {
      if ( defined $tinfo->{'check_testcase_failed'} )
      {
	push(@check_testcases, $tinfo->{'name'});
      }
    }

    if ( @check_testcases )
    {
      print "Check of testcase failed for: ";
      print join(" ", @check_testcases);
      print "\n\n";
    }
  }

  if ( $tot_failed != 0 || $found_problems)
  {
    dtr_error("there were failing test cases");
  }
}

##############################################################################
#
#  Text formatting
#
##############################################################################

sub dtr_print_line () {
  print '-' x 80, "\n";
}

sub dtr_print_thick_line () {
  print '=' x 80, "\n";
}

sub dtr_print_header () {
  print "DEFAULT STORAGE ENGINE: $::opt_engine\n";
  if ( $::opt_timer )
  {
    printf "%-61s%-9s%10s\n","TEST","RESULT","TIME (ms)";
  }
  else
  {
    print "TEST                           RESULT\n";
  }
  dtr_print_line();
  print "\n";
}


##############################################################################
#
#  Log and reporting functions
#
##############################################################################

use IO::File;

my $log_file_ref= undef;

sub dtr_log_init ($) {
  my ($filename)= @_;

  dtr_error("Log is already open") if defined $log_file_ref;

  $log_file_ref= IO::File->new($filename, "a") or
    dtr_warning("Could not create logfile $filename: $!");
}

sub _dtr_log (@) {
  print $log_file_ref join(" ", @_),"\n"
    if defined $log_file_ref;
}

sub dtr_report (@) {
  # Print message to screen and log
  _dtr_log(@_);
  print join(" ", @_),"\n";
}

sub dtr_warning (@) {
  # Print message to screen and log
  _dtr_log("WARNING: ", @_);
  print STDERR "drizzle-test-run: WARNING: ",join(" ", @_),"\n";
}

sub dtr_error (@) {
  # Print message to screen and log
  _dtr_log("ERROR: ", @_);
  print STDERR "drizzle-test-run: *** ERROR: ",join(" ", @_),"\n";
  dtr_exit(1);
}

sub dtr_child_error (@) {
  # Print message to screen and log
  _dtr_log("ERROR(child): ", @_);
  print STDERR "drizzle-test-run: *** ERROR(child): ",join(" ", @_),"\n";
  exit(1);
}

sub dtr_debug (@) {
  # Only print if --script-debug is used
  if ( $::opt_script_debug )
  {
    _dtr_log("###: ", @_);
    print STDERR "####: ",join(" ", @_),"\n";
  }
}

sub dtr_verbose (@) {
  # Always print to log, print to screen only when --verbose is used
  _dtr_log("> ",@_);
  if ( $::opt_verbose )
  {
    print STDERR "> ",join(" ", @_),"\n";
  }
}

1;
