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

# Private IM-related operations.

sub dtr_im_kill_process ($$$$);
sub dtr_im_load_pids ($);
sub dtr_im_terminate ($);
sub dtr_im_check_alive ($);
sub dtr_im_check_main_alive ($);
sub dtr_im_check_angel_alive ($);
sub dtr_im_check_drizzleds_alive ($);
sub dtr_im_check_drizzled_alive ($);
sub dtr_im_cleanup ($);
sub dtr_im_rm_file ($);
sub dtr_im_errlog ($);
sub dtr_im_kill ($);
sub dtr_im_wait_for_connection ($$$);
sub dtr_im_wait_for_drizzled($$$);

# Public IM-related operations.

sub dtr_im_start ($$);
sub dtr_im_stop ($);

##############################################################################
#
#  Private operations.
#
##############################################################################

sub dtr_im_kill_process ($$$$) {
  my $pid_lst= shift;
  my $signal= shift;
  my $total_retries= shift;
  my $timeout= shift;

  my %pids;

  foreach my $pid ( @{$pid_lst} )
  {
    $pids{$pid}= 1;
  }

  for ( my $cur_attempt= 1; $cur_attempt <= $total_retries; ++$cur_attempt )
  {
    foreach my $pid ( keys %pids )
    {
      dtr_debug("Sending $signal to $pid...");

      kill($signal, $pid);

      unless ( kill (0, $pid) )
      {
        dtr_debug("Process $pid died.");
        delete $pids{$pid};
      }
    }

    return if scalar keys %pids == 0;

    dtr_debug("Sleeping $timeout second(s) waiting for processes to die...");

    sleep($timeout);
  }

  dtr_debug("Process(es) " .
            join(' ', keys %pids) .
            " is still alive after $total_retries " .
            "of sending signal $signal.");
}

###########################################################################

sub dtr_im_load_pids($) {
  my $im= shift;

  dtr_debug("Loading PID files...");

  # Obtain drizzled-process pids.

  my $instances = $im->{'instances'};

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    dtr_debug("IM-guarded drizzled[$idx] PID file: '" .
              $instances->[$idx]->{'path_pid'} . "'.");

    my $drizzled_pid;

    if ( -r $instances->[$idx]->{'path_pid'} )
    {
      $drizzled_pid= dtr_get_pid_from_file($instances->[$idx]->{'path_pid'});
      dtr_debug("IM-guarded drizzled[$idx] PID: $drizzled_pid.");
    }
    else
    {
      $drizzled_pid= undef;
      dtr_debug("IM-guarded drizzled[$idx]: no PID file.");
    }

    $instances->[$idx]->{'pid'}= $drizzled_pid;
  }

  # Re-read Instance Manager PIDs from the file, since during tests Instance
  # Manager could have been restarted, so its PIDs could have been changed.

  #   - IM-main

  dtr_debug("IM-main PID file: '$im->{path_pid}'.");

  if ( -f $im->{'path_pid'} )
  {
    $im->{'pid'} =
      dtr_get_pid_from_file($im->{'path_pid'});

    dtr_debug("IM-main PID: $im->{pid}.");
  }
  else
  {
    dtr_debug("IM-main: no PID file.");
    $im->{'pid'}= undef;
  }

  #   - IM-angel

  dtr_debug("IM-angel PID file: '$im->{path_angel_pid}'.");

  if ( -f $im->{'path_angel_pid'} )
  {
    $im->{'angel_pid'} =
      dtr_get_pid_from_file($im->{'path_angel_pid'});

    dtr_debug("IM-angel PID: $im->{'angel_pid'}.");
  }
  else
  {
    dtr_debug("IM-angel: no PID file.");
    $im->{'angel_pid'} = undef;
  }
}

###########################################################################

sub dtr_im_terminate($) {
  my $im= shift;

  # Load pids from pid-files. We should do it first of all, because IM deletes
  # them on shutdown.

  dtr_im_load_pids($im);

  dtr_debug("Shutting Instance Manager down...");

  # Ignoring SIGCHLD so that all children could rest in peace.

  start_reap_all();

  # Send SIGTERM to IM-main.

  if ( defined $im->{'pid'} )
  {
    dtr_debug("IM-main pid: $im->{pid}.");
    dtr_debug("Stopping IM-main...");

    dtr_im_kill_process([ $im->{'pid'} ], 'TERM', 10, 1);
  }
  else
  {
    dtr_debug("IM-main pid: n/a.");
  }

  # If IM-angel was alive, wait for it to die.

  if ( defined $im->{'angel_pid'} )
  {
    dtr_debug("IM-angel pid: $im->{'angel_pid'}.");
    dtr_debug("Waiting for IM-angel to die...");

    my $total_attempts= 10;

    for ( my $cur_attempt=1; $cur_attempt <= $total_attempts; ++$cur_attempt )
    {
      unless ( kill (0, $im->{'angel_pid'}) )
      {
        dtr_debug("IM-angel died.");
        last;
      }

      sleep(1);
    }
  }
  else
  {
    dtr_debug("IM-angel pid: n/a.");
  }

  stop_reap_all();

  # Re-load PIDs.

  dtr_im_load_pids($im);
}

###########################################################################

sub dtr_im_check_alive($) {
  my $im= shift;

  dtr_debug("Checking whether IM-components are alive...");

  return 1 if dtr_im_check_main_alive($im);

  return 1 if dtr_im_check_angel_alive($im);

  return 1 if dtr_im_check_drizzleds_alive($im);

  return 0;
}

###########################################################################

sub dtr_im_check_main_alive($) {
  my $im= shift;

  # Check that the process, that we know to be IM's, is dead.

  if ( defined $im->{'pid'} )
  {
    if ( kill (0, $im->{'pid'}) )
    {
      dtr_debug("IM-main (PID: $im->{pid}) is alive.");
      return 1;
    }
    else
    {
      dtr_debug("IM-main (PID: $im->{pid}) is dead.");
    }
  }
  else
  {
    dtr_debug("No PID file for IM-main.");
  }

  # Check that IM does not accept client connections.

  if ( dtr_ping_port($im->{'port'}) )
  {
    dtr_debug("IM-main (port: $im->{port}) " .
              "is accepting connections.");

    dtr_im_errlog("IM-main is accepting connections on port " .
                  "$im->{port}, but there is no " .
                  "process information.");
    return 1;
  }
  else
  {
    dtr_debug("IM-main (port: $im->{port}) " .
              "does not accept connections.");
    return 0;
  }
}

###########################################################################

sub dtr_im_check_angel_alive($) {
  my $im= shift;

  # Check that the process, that we know to be the Angel, is dead.

  if ( defined $im->{'angel_pid'} )
  {
    if ( kill (0, $im->{'angel_pid'}) )
    {
      dtr_debug("IM-angel (PID: $im->{angel_pid}) is alive.");
      return 1;
    }
    else
    {
      dtr_debug("IM-angel (PID: $im->{angel_pid}) is dead.");
      return 0;
    }
  }
  else
  {
    dtr_debug("No PID file for IM-angel.");
    return 0;
  }
}

###########################################################################

sub dtr_im_check_drizzleds_alive($) {
  my $im= shift;

  dtr_debug("Checking for IM-guarded drizzled instances...");

  my $instances = $im->{'instances'};

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    dtr_debug("Checking drizzled[$idx]...");

    return 1
      if dtr_im_check_drizzled_alive($instances->[$idx]);
  }
}

###########################################################################

sub dtr_im_check_drizzled_alive($) {
  my $drizzled_instance= shift;

  # Check that the process is dead.

  if ( defined $drizzled_instance->{'pid'} )
  {
    if ( kill (0, $drizzled_instance->{'pid'}) )
    {
      dtr_debug("drizzled instance (PID: $drizzled_instance->{pid}) is alive.");
      return 1;
    }
    else
    {
      dtr_debug("drizzled instance (PID: $drizzled_instance->{pid}) is dead.");
    }
  }
  else
  {
    dtr_debug("No PID file for drizzled instance.");
  }

  # Check that drizzled does not accept client connections.

  if ( dtr_ping_port($drizzled_instance->{'port'}) )
  {
    dtr_debug("drizzled instance (port: $drizzled_instance->{port}) " .
              "is accepting connections.");

    dtr_im_errlog("drizzled is accepting connections on port " .
                  "$drizzled_instance->{port}, but there is no " .
                  "process information.");
    return 1;
  }
  else
  {
    dtr_debug("drizzled instance (port: $drizzled_instance->{port}) " .
              "does not accept connections.");
    return 0;
  }
}

###########################################################################

sub dtr_im_cleanup($) {
  my $im= shift;

  dtr_im_rm_file($im->{'path_pid'});
  dtr_im_rm_file($im->{'path_sock'});

  dtr_im_rm_file($im->{'path_angel_pid'});

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    dtr_im_rm_file($im->{'instances'}->[$idx]->{'path_pid'});
    dtr_im_rm_file($im->{'instances'}->[$idx]->{'path_sock'});
  }
}

###########################################################################

sub dtr_im_rm_file($)
{
  my $file_path= shift;

  if ( -f $file_path )
  {
    dtr_debug("Removing '$file_path'...");

    unless ( unlink($file_path) )
    {
      dtr_warning("Can not remove '$file_path'.")
    }
  }
  else
  {
    dtr_debug("File '$file_path' does not exist already.");
  }
}

###########################################################################

sub dtr_im_errlog($) {
  my $msg= shift;

  # Complain in error log so that a warning will be shown.
  # 
  # TODO: unless BUG#20761 is fixed, we will print the warning to stdout, so
  # that it can be seen on console and does not produce pushbuild error.

  # my $errlog= "$opt_vardir/log/drizzle-test-run.pl.err";
  # 
  # open (ERRLOG, ">>$errlog") ||
  #   dtr_error("Can not open error log ($errlog)");
  # 
  # my $ts= localtime();
  # print ERRLOG
  #   "Warning: [$ts] $msg\n";
  # 
  # close ERRLOG;

  my $ts= localtime();
  print "Warning: [$ts] $msg\n";
}

###########################################################################

sub dtr_im_kill($) {
  my $im= shift;

  # Re-load PIDs. That can be useful because some processes could have been
  # restarted.

  dtr_im_load_pids($im);

  # Ignoring SIGCHLD so that all children could rest in peace.

  start_reap_all();

  # Kill IM-angel first of all.

  if ( defined $im->{'angel_pid'} )
  {
    dtr_debug("Killing IM-angel (PID: $im->{angel_pid})...");
    dtr_im_kill_process([ $im->{'angel_pid'} ], 'KILL', 10, 1)
  }
  else
  {
    dtr_debug("IM-angel is dead.");
  }

  # Re-load PIDs again.

  dtr_im_load_pids($im);

  # Kill IM-main.
  
  if ( defined $im->{'pid'} )
  {
    dtr_debug("Killing IM-main (PID: $im->pid})...");
    dtr_im_kill_process([ $im->{'pid'} ], 'KILL', 10, 1);
  }
  else
  {
    dtr_debug("IM-main is dead.");
  }

  # Re-load PIDs again.

  dtr_im_load_pids($im);

  # Kill guarded drizzled instances.

  my @drizzled_pids;

  dtr_debug("Collecting PIDs of drizzled instances to kill...");

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    my $pid= $im->{'instances'}->[$idx]->{'pid'};

    unless ( defined $pid )
    {
      next;
    }

    dtr_debug("  - IM-guarded drizzled[$idx] PID: $pid.");

    push (@drizzled_pids, $pid);
  }

  if ( scalar @drizzled_pids > 0 )
  {
    dtr_debug("Killing IM-guarded drizzled instances...");
    dtr_im_kill_process(\@drizzled_pids, 'KILL', 10, 1);
  }

  # That's all.

  stop_reap_all();
}

##############################################################################

sub dtr_im_wait_for_connection($$$) {
  my $im= shift;
  my $total_attempts= shift;
  my $connect_timeout= shift;

  dtr_debug("Waiting for IM on port $im->{port} " .
            "to start accepting connections...");

  for ( my $cur_attempt= 1; $cur_attempt <= $total_attempts; ++$cur_attempt )
  {
    dtr_debug("Trying to connect to IM ($cur_attempt of $total_attempts)...");

    if ( dtr_ping_port($im->{'port'}) )
    {
      dtr_debug("IM is accepting connections " .
                "on port $im->{port}.");
      return 1;
    }

    dtr_debug("Sleeping $connect_timeout...");
    sleep($connect_timeout);
  }

  dtr_debug("IM does not accept connections " .
            "on port $im->{port} after " .
            ($total_attempts * $connect_timeout) . " seconds.");

  return 0;
}

##############################################################################

sub dtr_im_wait_for_drizzled($$$) {
  my $drizzled= shift;
  my $total_attempts= shift;
  my $connect_timeout= shift;

  dtr_debug("Waiting for IM-guarded drizzled on port $drizzled->{port} " .
            "to start accepting connections...");

  for ( my $cur_attempt= 1; $cur_attempt <= $total_attempts; ++$cur_attempt )
  {
    dtr_debug("Trying to connect to drizzled " .
              "($cur_attempt of $total_attempts)...");

    if ( dtr_ping_port($drizzled->{'port'}) )
    {
      dtr_debug("drizzled is accepting connections " .
                "on port $drizzled->{port}.");
      return 1;
    }

    dtr_debug("Sleeping $connect_timeout...");
    sleep($connect_timeout);
  }

  dtr_debug("drizzled does not accept connections " .
            "on port $drizzled->{port} after " .
            ($total_attempts * $connect_timeout) . " seconds.");

  return 0;
}

##############################################################################
#
#  Public operations.
#
##############################################################################

sub dtr_im_start($$) {
  my $im = shift;
  my $opts = shift;

  dtr_debug("Starting Instance Manager...");

  my $args;
  dtr_init_args(\$args);
  dtr_add_arg($args, "--defaults-file=%s", $im->{'defaults_file'});

  foreach my $opt ( @{$opts} )
  {
    dtr_add_arg($args, $opt);
  }

  $im->{'spawner_pid'} =
    dtr_spawn(
      $::exe_im,                        # path to the executable
      $args,                            # cmd-line args
      '',                               # stdin
      $im->{'path_log'},                # stdout
      $im->{'path_err'},                # stderr
      '',                               # pid file path (not used)
      { append_log_file => 1 }          # append log files
      );

  unless ( $im->{'spawner_pid'} )
  {
    dtr_error('Could not start Instance Manager.')
  }

  # Instance Manager can be run in daemon mode. In this case, it creates
  # several processes and the parent process, created by dtr_spawn(), exits just
  # after start. So, we have to obtain Instance Manager PID from the PID file.

  dtr_debug("Waiting for IM to create PID file (" .
            "path: '$im->{path_pid}'; " .
            "timeout: $im->{start_timeout})...");

  unless ( sleep_until_file_created($im->{'path_pid'},
                                    $im->{'start_timeout'},
                                    -1) ) # real PID is still unknown
  {
    dtr_debug("IM has not created PID file in $im->{start_timeout} secs.");
    dtr_debug("Aborting test suite...");

    dtr_kill_leftovers();

    dtr_report("IM has not created PID file in $im->{start_timeout} secs.");
    return 0;
  }

  $im->{'pid'}= dtr_get_pid_from_file($im->{'path_pid'});

  dtr_debug("Instance Manager started. PID: $im->{pid}.");

  # Wait until we can connect to IM.

  my $IM_CONNECT_TIMEOUT= 30;

  unless ( dtr_im_wait_for_connection($im,
                                      $IM_CONNECT_TIMEOUT, 1) )
  {
    dtr_debug("Can not connect to Instance Manager " .
              "in $IM_CONNECT_TIMEOUT seconds after start.");
    dtr_debug("Aborting test suite...");

    dtr_kill_leftovers();

    dtr_report("Can not connect to Instance Manager " .
               "in $IM_CONNECT_TIMEOUT seconds after start.");
    return 0;
  }

  # Wait for IM to start guarded instances:
  #   - wait for PID files;

  dtr_debug("Waiting for guarded drizzleds instances to create PID files...");

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    my $drizzled= $im->{'instances'}->[$idx];

    if ( exists $drizzled->{'nonguarded'} )
    {
      next;
    }

    dtr_debug("Waiting for drizzled[$idx] to create PID file (" .
              "path: '$drizzled->{path_pid}'; " .
              "timeout: $drizzled->{start_timeout})...");

    unless ( sleep_until_file_created($drizzled->{'path_pid'},
                                      $drizzled->{'start_timeout'},
                                      -1) ) # real PID is still unknown
    {
      dtr_debug("drizzled[$idx] has not created PID file in " .
                 "$drizzled->{start_timeout} secs.");
      dtr_debug("Aborting test suite...");

      dtr_kill_leftovers();

      dtr_report("drizzled[$idx] has not created PID file in " .
                 "$drizzled->{start_timeout} secs.");
      return 0;
    }

    dtr_debug("PID file for drizzled[$idx] ($drizzled->{path_pid} created.");
  }

  # Wait until we can connect to guarded drizzled-instances
  # (in other words -- wait for IM to start guarded instances).

  dtr_debug("Waiting for guarded drizzleds to start accepting connections...");

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    my $drizzled= $im->{'instances'}->[$idx];

    if ( exists $drizzled->{'nonguarded'} )
    {
      next;
    }

    dtr_debug("Waiting for drizzled[$idx] to accept connection...");

    unless ( dtr_im_wait_for_drizzled($drizzled, 30, 1) )
    {
      dtr_debug("Can not connect to drizzled[$idx] " .
                "in $IM_CONNECT_TIMEOUT seconds after start.");
      dtr_debug("Aborting test suite...");

      dtr_kill_leftovers();

      dtr_report("Can not connect to drizzled[$idx] " .
                 "in $IM_CONNECT_TIMEOUT seconds after start.");
      return 0;
    }

    dtr_debug("drizzled[$idx] started.");
  }

  dtr_debug("Instance Manager and its components are up and running.");

  return 1;
}

##############################################################################

sub dtr_im_stop($) {
  my $im= shift;

  dtr_debug("Stopping Instance Manager...");

  # Try graceful shutdown.

  dtr_im_terminate($im);

  # Check that all processes died.

  unless ( dtr_im_check_alive($im) )
  {
    dtr_debug("Instance Manager has been stopped successfully.");
    dtr_im_cleanup($im);
    return 1;
  }

  # Instance Manager don't want to die. We should kill it.

  dtr_im_errlog("Instance Manager did not shutdown gracefully.");

  dtr_im_kill($im);

  # Check again that all IM-related processes have been killed.

  my $im_is_alive= dtr_im_check_alive($im);

  dtr_im_cleanup($im);

  if ( $im_is_alive )
  {
    dtr_debug("Can not kill Instance Manager or its children.");
    return 0;
  }

  dtr_debug("Instance Manager has been killed successfully.");
  return 1;
}

###########################################################################

1;
