#!/usr/bin/env perl

use strict;
use warnings;
use English qw(-no_match_vars);

my ($file, $attrib, $cmp, $val, $val2) = @ARGV;
if ( !$file || !$attrib ) {
   warn "Usage: check-query-log-values.pl FILE ATTRIBUTE [CMP VALUE [VALUE]]\n";
   exit 1;
}

# Slurp the file.  There should be only 1 event.
open my $fh, "<", $file or die "Cannot open $file: $OS_ERROR";
my $event_text = '';
{
   local $INPUT_RECORD_SEPARATOR = "\n#\n";
   while ( defined(my $event = <$fh>) ) {
      $event_text = $event;
   }
}
close $fh;
my @event_lines = split /\n/, $event_text;

my $event = parse_event(@event_lines);

my @attribs = split /,/, $attrib;
my @vals    = split /,/, $val if $val;
my @vals2   = split /,/, $val2 if $val2;

if ( $val ) {
   die "The number of ATTRIBUTEs and VALUEs does not match"
      if $#attribs != $#vals;
}

print "Checking attributes and values of query $event->{arg}\n";

for my $i (0..$#attribs) {
   my $attrib = lc $attribs[$i];
   my $val    = $vals[$i];
   my $val2   = $vals2[$i];

   if ( $attrib eq 'all' ) {
      dump_event($event);
   }
   else {
      die "A CMP argument is required if ATTRIBUTE is not ALL"
         unless $cmp;
      die "A VALUE argument is required if ATTRIBUTE is not ALL"
         unless defined $val;

      print "ERROR: attribute $attrib does not exist.\n"
         unless exists $event->{$attrib};

      $cmp = lc $cmp;

      my $ok = 0;
      my $event_val = $event->{$attrib};
      if ( $cmp eq '=' || $cmp eq 'equals' ) {
         $ok = 1 if defined $event_val && $event_val eq $val;
      }
      elsif ( $cmp eq 'matches' ) {
         $ok = 1 if defined $event_val && $event_val =~ m/$val/;
      }
      elsif ( $cmp eq 'between' ) {
         die "I need a second VALUE argument if CMP is BETWEEN"
            unless $val2;
         $ok = 1 if defined $event_val
            && $val <= $event_val && $event_val <= $val2;
      }
      else {
         die "Unknown CMP: $cmp";
      }

      if ( $ok ) {
         # Don't print the matches pattern becaues it's probably some
         # variable value like a timestamp.
         print "$attrib value $cmp "
            . ($cmp eq 'matches' ? "" : "$val ")
            . ($cmp eq 'between' ? "and $val2 " : "")
            . "OK\n";
      }
      else {
         print "$attrib value $event_val does not '$cmp' $val"
            . ($cmp eq 'between' ? " and $val2" : "") . "\n"
            . "Event dump:\n";
         dump_event($event);
      }
   }
}

sub parse_event {
   my (@event_lines) = @_;
   die "I need a event_lines argument" unless @event_lines;

   my $rs = pop @event_lines;
   if ( $rs ne "#" ) {
      print "ERROR: Event does not end with the # record separator.\n";
   }

   my @props;
   my $arg = '';
   my $lineno = 1;
   foreach my $line ( @event_lines ) { 
      next if $line =~ m/^$/;
      if ( $lineno <= 4 ) { # ts, ints, floats and bools
         push @props, $line =~ m/([a-z_]+)=(\S+)/g;
      }
      elsif ( $lineno == 5 ) { # strings
         push @props, $line =~ m/([a-z_]+)="([^"]*)"/;
      }
      else { # query
         $arg .= $line;
      }
      $lineno++;
   }

   chomp $arg;
   push @props, 'arg', $arg;

   my $event = { @props };

   return $event;
}

sub dump_event {
   my ($event) = @_;
   foreach my $attrib ( sort keys %$event ) {
      print "$attrib=$event->{$attrib}\n";
   }
}
