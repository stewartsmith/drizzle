# -*- cperl -*-
# Copyright (C) 2005-2006 MySQL AB
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

use Errno;
use strict;

sub dtr_init_timers ();
sub dtr_timer_start($$$);
sub dtr_timer_stop($$);
sub dtr_timer_stop_all($);


##############################################################################
#
#  Initiate the structure shared by all timers
#
##############################################################################

sub dtr_init_timers () {
  my $timers = { timers => {}, pids => {}};
  return $timers;
}


##############################################################################
#
#  Start, stop and poll a timer
#
#  As alarm() isn't portable to Windows, we use separate processes to
#  implement timers.
#
##############################################################################

sub dtr_timer_start($$$) {
  my ($timers,$name,$duration)= @_;

  if ( exists $timers->{'timers'}->{$name} )
  {
    # We have an old running timer, kill it
    dtr_warning("There is an old timer running");
    dtr_timer_stop($timers,$name);
  }

 FORK:
  {
    my $tpid= fork();

    if ( ! defined $tpid )
    {
      if ( $! == $!{EAGAIN} )           # See "perldoc Errno"
      {
        dtr_warning("Got EAGAIN from fork(), sleep 1 second and redo");
        sleep(1);
        redo FORK;
      }
      else
      {
        dtr_error("can't fork timer, error: $!");
      }
    }

    if ( $tpid )
    {
      # Parent, record the information
      dtr_verbose("Starting timer for '$name',",
		  "duration: $duration, pid: $tpid");
      $timers->{'timers'}->{$name}->{'pid'}= $tpid;
      $timers->{'timers'}->{$name}->{'duration'}= $duration;
      $timers->{'pids'}->{$tpid}= $name;
    }
    else
    {
      # Child, install signal handlers and sleep for "duration"

      # Don't do the ^C cleanup in the timeout child processes!
      # There is actually a race here, if we get ^C after fork(), but before
      # clearing the signal handler.
      $SIG{INT}= 'DEFAULT';

      $SIG{TERM}= sub {
	dtr_verbose("timer $$ woke up, exiting!");
	exit(0);
      };

      $0= "dtr_timer(timers,$name,$duration)";
      sleep($duration);
      dtr_verbose("timer $$ expired after $duration seconds");
      exit(0);
    }
  }
}


sub dtr_timer_stop ($$) {
  my ($timers,$name)= @_;

  if ( exists $timers->{'timers'}->{$name} )
  {
    my $tpid= $timers->{'timers'}->{$name}->{'pid'};
    dtr_verbose("Stopping timer for '$name' with pid $tpid");

    # FIXME as Cygwin reuses pids fast, maybe check that is
    # the expected process somehow?!
    kill(15, $tpid);

    # As the timers are so simple programs, we trust them to terminate,
    # and use blocking wait for it. We wait just to avoid a zombie.
    waitpid($tpid,0);

    delete $timers->{'timers'}->{$name}; # Remove the timer information
    delete $timers->{'pids'}->{$tpid};   # and PID reference

    return 1;
  }

  dtr_error("Asked to stop timer '$name' not started");
}


sub dtr_timer_stop_all ($) {
  my $timers= shift;

  foreach my $name ( keys %{$timers->{'timers'}} )
  {
    dtr_timer_stop($timers, $name);
  }
  return 1;
}


sub dtr_timer_timeout ($$) {
  my ($timers,$pid)= @_;

  return "" unless exists $timers->{'pids'}->{$pid};

  # Got a timeout(the process with $pid is recorded as being a timer)
  # return the name of the timer
  return $timers->{'pids'}->{$pid};
}

1;
