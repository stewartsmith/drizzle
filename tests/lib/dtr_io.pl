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

sub dtr_get_pid_from_file ($);
sub dtr_get_opts_from_file ($);
sub dtr_fromfile ($);
sub dtr_tofile ($@);
sub dtr_tonewfile($@);
sub dtr_lastlinefromfile($);
sub dtr_appendfile_to_file ($$);
sub dtr_grab_file($);


##############################################################################
#
#  
#
##############################################################################

sub dtr_get_pid_from_file ($) {
  my $pid_file_path=  shift;
  my $TOTAL_ATTEMPTS= 30;
  my $timeout= 1;

  # We should read from the file until we get correct pid. As it is
  # stated in BUG#21884, pid file can be empty at some moment. So, we should
  # read it until we get valid data.

  for (my $cur_attempt= 1; $cur_attempt <= $TOTAL_ATTEMPTS; ++$cur_attempt)
  {
    dtr_debug("Reading pid file '$pid_file_path' " .
              "($cur_attempt of $TOTAL_ATTEMPTS)...");

    open(FILE, '<', $pid_file_path)
      or dtr_error("can't open file \"$pid_file_path\": $!");

    # Read pid number from file
    my $pid= <FILE>;
    chomp $pid;
    close FILE;

    return $pid if $pid=~ /^(\d+)/;

    dtr_debug("Pid file '$pid_file_path' does not yet contain pid number.\n" .
              "Sleeping $timeout second(s) more...");

    sleep($timeout);
  }

  dtr_error("Pid file '$pid_file_path' is corrupted. " .
            "Can not retrieve PID in " .
            ($timeout * $TOTAL_ATTEMPTS) . " seconds.");
}

sub dtr_get_opts_from_file ($) {
  my $file=  shift;

  open(FILE,"<",$file) or dtr_error("can't open file \"$file\": $!");
  my @args;
  while ( <FILE> )
  {
    chomp;

    #    --set-variable=init_connect=set @a='a\\0c'
    s/^\s+//;                           # Remove leading space
    s/\s+$//;                           # Remove ending space

    # This is strange, but we need to fill whitespace inside
    # quotes with something, to remove later. We do this to
    # be able to split on space. Else, we have trouble with
    # options like 
    #
    #   --someopt="--insideopt1 --insideopt2"
    #
    # But still with this, we are not 100% sure it is right,
    # we need a shell to do it right.

#    print STDERR "\n";
#    print STDERR "AAA: $_\n";

    s/\'([^\'\"]*)\'/unspace($1,"\x0a")/ge;
    s/\"([^\'\"]*)\"/unspace($1,"\x0b")/ge;
    s/\'([^\'\"]*)\'/unspace($1,"\x0a")/ge;
    s/\"([^\'\"]*)\"/unspace($1,"\x0b")/ge;

#    print STDERR "BBB: $_\n";

#    foreach my $arg (/(--?\w.*?)(?=\s+--?\w|$)/)

    # FIXME ENV vars should be expanded!!!!

    foreach my $arg (split(/[ \t]+/))
    {
      $arg =~ tr/\x11\x0a\x0b/ \'\"/;     # Put back real chars
      # The outermost quotes has to go
      $arg =~ s/^([^\'\"]*)\'(.*)\'([^\'\"]*)$/$1$2$3/
        or $arg =~ s/^([^\'\"]*)\"(.*)\"([^\'\"]*)$/$1$2$3/;
      $arg =~ s/\\\\/\\/g;

      $arg =~ s/\$\{(\w+)\}/envsubst($1)/ge;
      $arg =~ s/\$(\w+)/envsubst($1)/ge;

#      print STDERR "ARG: $arg\n";
      # Do not pass empty string since my_getopt is not capable to handle it.
      if (length($arg))
      {
        push(@args, $arg)
      }
    }
  }
  close FILE;
  return \@args;
}

sub envsubst {
  my $string= shift;

  if ( ! defined $ENV{$string} )
  {
    dtr_error("opt file referense \$$string that is unknown");
  }

  return $ENV{$string};
}

sub unspace {
  my $string= shift;
  my $quote=  shift;
  $string =~ s/[ \t]/\x11/g;
  return "$quote$string$quote";
}

# Read a whole file, stripping leading and trailing whitespace.
sub dtr_fromfile ($) {
  my $file=  shift;

  open(FILE,"<",$file) or dtr_error("can't open file \"$file\": $!");
  my $text= join('', <FILE>);
  close FILE;
  $text =~ s/^\s+//;                    # Remove starting space, incl newlines
  $text =~ s/\s+$//;                    # Remove ending space, incl newlines
  return $text;
}

sub dtr_lastlinefromfile ($) {
  my $file=  shift;
  my $text;

  open(FILE,"<",$file) or dtr_error("can't open file \"$file\": $!");
  while (my $line= <FILE>)
  {
    $text= $line;
  }
  close FILE;
  return $text;
}


sub dtr_tofile ($@) {
  my $file=  shift;

  open(FILE,">>",$file) or dtr_error("can't open file \"$file\": $!");
  print FILE join("", @_);
  close FILE;
}

sub dtr_tonewfile ($@) {
  my $file=  shift;

  open(FILE,">",$file) or dtr_error("can't open file \"$file\": $!");
  print FILE join("", @_);
  close FILE;
}

sub dtr_appendfile_to_file ($$) {
  my $from_file=  shift;
  my $to_file=  shift;

  open(TOFILE,">>",$to_file) or dtr_error("can't open file \"$to_file\": $!");
  open(FROMFILE,"<",$from_file)
    or dtr_error("can't open file \"$from_file\": $!");
  print TOFILE while (<FROMFILE>);
  close FROMFILE;
  close TOFILE;
}

# Read a whole file verbatim.
sub dtr_grab_file($) {
  my $file= shift;
  open(FILE, '<', $file)
    or return undef;
  local $/= undef;
  my $data= scalar(<FILE>);
  close FILE;
  return $data;
}


1;
