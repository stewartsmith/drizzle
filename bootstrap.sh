#!/bin/bash
# 
# Copyright (C) 2012 Brian Aker
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
# 
#     * The names of its contributors may not be used to endorse or
# promote products derived from this software without specific prior
# written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Environment Variables that will influence the build:
#   AUTOMAKE
#   AUTORECONF
#   LIBTOOLIZE
#   MAKE
#   MAKE_TARGET
#   PREFIX
#   TESTS_ENVIRONMENT
#   VERBOSE
#   WARNINGS
#

command_not_found_handle ()
{
  echo "Command not found: '$@'"
  exit 127
}

die () { echo "$@"; exit 1; }

command_exists ()
{
  type "$1" &> /dev/null ;
}

rebuild_host_os ()
{
  HOST_OS="${UNAME_MACHINE_ARCH}-${VENDOR}-${VENDOR_DISTRIBUTION}-${VENDOR_RELEASE}-${UNAME_KERNEL}-${UNAME_KERNEL_RELEASE}"
  if [ -z "$1" ]; then
    if $VERBOSE; then
      echo "HOST_OS=$HOST_OS"
    fi
  fi
}

#  Valid values are: darwin,fedora,rhel,ubuntu
set_VENDOR_DISTRIBUTION ()
{
  local dist=`echo "$1" | tr '[A-Z]' '[a-z]'`
  case "$dist" in
    darwin)
      VENDOR_DISTRIBUTION='darwin'
      ;;
    fedora)
      VENDOR_DISTRIBUTION='fedora'
      ;;
    rhel)
      VENDOR_DISTRIBUTION='rhel'
      ;;
    ubuntu)
      VENDOR_DISTRIBUTION='ubuntu'
      ;;
    opensuse)
      VENDOR_DISTRIBUTION='opensuse'
      ;;
    *)
      die "$LINENO: attempt to set an invalid VENDOR_DISTRIBUTION=$dist"
      ;;
  esac
}

set_VENDOR_RELEASE ()
{
  local release=`echo "$1" | tr '[A-Z]' '[a-z]'`
  case "$VENDOR_DISTRIBUTION" in
    darwin)
      VENDOR_RELEASE='mountain'
      ;;
    fedora)
      VENDOR_RELEASE="$release"
      ;;
    rhel)
      VENDOR_RELEASE="$release"
      ;;
    ubuntu)
      VENDOR_RELEASE="$release"
      ;;
    opensuse)
      VENDOR_RELEASE="$release"
      ;;
    unknown)
      die "$LINENO: attempt to set VENDOR_RELEASE without setting VENDOR_DISTRIBUTION"
      ;;
    *)
      die "$LINENO: attempt to set with an invalid VENDOR_DISTRIBUTION=$VENDOR_DISTRIBUTION"
      ;;
  esac
}


#  Valid values are: apple, redhat, centos, canonical
set_VENDOR ()
{
  local vendor=`echo "$1" | tr '[A-Z]' '[a-z]'`

  case $vendor in
    apple)
      VENDOR="apple"
      ;;
    redhat)
      VENDOR="redhat"
      ;;
    centos)
      VENDOR="centos"
      ;;
    canonical)
      VENDOR="canonical"
      ;;
    suse)
      VENDOR="suse"
      ;;
    *)
      die "$LINENO: An attempt was made to set an invalid VENDOR=$_vendor"
      ;;
  esac

  set_VENDOR_DISTRIBUTION $2
  set_VENDOR_RELEASE $3
}

determine_target_platform ()
{
  UNAME_MACHINE_ARCH=`(uname -m) 2>/dev/null` || UNAME_MACHINE_ARCH=unknown
  UNAME_KERNEL=`(uname -s) 2>/dev/null`  || UNAME_SYSTEM=unknown
  UNAME_KERNEL_RELEASE=`(uname -r) 2>/dev/null` || UNAME_KERNEL_RELEASE=unknown

  if [[ $(uname) == "Darwin" ]]; then
    set_VENDOR "apple" "darwin" "mountain"
  elif [[ -f "/etc/fedora-release" ]]; then 
    local fedora_version=`cat /etc/fedora-release | awk ' { print $3 } '`
    set_VENDOR "redhat" "fedora" $fedora_version
    if [[ "x$VENDOR_RELEASE" == "x17" ]]; then
      AUTORECONF_REBUILD_HOST=true
    fi
  elif [[ -f "/etc/centos-release" ]]; then
    local centos_version= `cat /etc/centos-release | awk ' { print $7 } '`
    set_VENDOR "centos" "rhel" $centos_version
  elif [[ -f "/etc/SuSE-release" ]]; then
    local suse_distribution=`head -1 /etc/SuSE-release | awk ' { print $1 } '`
    local suse_version=`head -1 /etc/SuSE-release | awk ' { print $2 } '`
    set_VENDOR "suse" "$suse_distribution" "$suse_version"
  elif [[ -f "/etc/redhat-release" ]]; then
    local rhel_version= `cat /etc/redhat-release | awk ' { print $7 } '`
    set_VENDOR "redhat" "rhel" $rhel_version
  elif [[ -f "/etc/lsb-release" ]]; then 
    local debian_DISTRIB_ID=`cat /etc/lsb-release | grep DISTRIB_ID | awk -F= ' { print $2 } '`
    local debian_version=`cat /etc/lsb-release | grep DISTRIB_CODENAME | awk -F= ' { print $2 } '`
    set_VENDOR "canonical" $debian_DISTRIB_ID $debian_version
    if [[ "x$VENDOR_RELEASE" == "xprecise" ]]; then
      AUTORECONF_REBUILD_HOST=true
    fi
  fi

  rebuild_host_os
}

run_configure ()
{
  # We will run autoreconf if we are required
  run_autoreconf_if_required

  # We always begin at the root of our build
  if [ ! popd ]; then
    die "$LINENO: Programmer error, we entered run_configure with a stacked directory"
  fi

  local BUILD_DIR="$1"
  if [[ -n "$BUILD_DIR" ]]; then
    rm -r -f $BUILD_DIR
    mkdir -p $BUILD_DIR
    safe_pushd $BUILD_DIR
  fi

  # Arguments for configure
  local CONFIGURE_ARG= 

  # Set ENV DEBUG in order to enable debugging
  if $DEBUG; then 
    CONFIGURE_ARG="--enable-debug"
  fi

  # Set ENV ASSERT in order to enable assert
  if [[ -n "$ASSERT" ]]; then 
    local ASSERT_ARG=
    ASSERT_ARG="--enable-assert"
    CONFIGURE_ARG="$ASSERT_ARG $CONFIGURE_ARG"
  fi

  # If we are executing on OSX use CLANG, otherwise only use it if we find it in the ENV
  case $HOST_OS in
    *-darwin-*)
      CC=clang CXX=clang++ $top_srcdir/configure $CONFIGURE_ARG || die "$LINENO: Cannot execute CC=clang CXX=clang++ configure $CONFIGURE_ARG $PREFIX_ARG"
      ;;
    rhel-5*)
      command_exists gcc44 || die "$LINENO: Could not locate gcc44"
      CC=gcc44 CXX=gcc44 $top_srcdir/configure $CONFIGURE_ARG $PREFIX_ARG || die "$LINENO: Cannot execute CC=gcc44 CXX=gcc44 configure $CONFIGURE_ARG $PREFIX_ARG"
      ;;
    *)
      $top_srcdir/configure $CONFIGURE_ARG $PREFIX_ARG || die "$LINENO: Cannot execute configure $CONFIGURE_ARG $PREFIX_ARG"
      ;;
  esac

  if [ ! -f 'Makefile' ]; then
    die "$LINENO: Programmer error, configure was run but no Makefile existed afterward"
  fi
}

setup_gdb_command () {
  GDB_TMPFILE=$(mktemp /tmp/gdb.XXXXXXXXXX)
  echo "set logging overwrite on" > $GDB_TMPFILE
  echo "set logging on" >> $GDB_TMPFILE
  echo "set environment LIBTEST_IN_GDB=1" >> $GDB_TMPFILE
  echo "run" >> $GDB_TMPFILE
  echo "thread apply all bt" >> $GDB_TMPFILE
  echo "quit" >> $GDB_TMPFILE
  GDB_COMMAND="gdb -f -batch -x $GDB_TMPFILE"
}

setup_valgrind_command () {
  VALGRIND_PROGRAM=`type -p valgrind`
  if [[ -n "$VALGRIND_PROGRAM" ]]; then
    VALGRIND_COMMAND="$VALGRIND_PROGRAM --error-exitcode=1 --leak-check=yes --show-reachable=yes --track-fds=yes --malloc-fill=A5 --free-fill=DE"
  fi
}

push_PREFIX_ARG ()
{
  if [[ -n "$PREFIX_ARG" ]]; then
    OLD_PREFIX_ARG=$PREFIX_ARG
    PREFIX_ARG=
  fi

  if [[ -n "$1" ]]; then
    PREFIX_ARG="--prefix=$1"
  fi
}

pop_PREFIX_ARG ()
{
  if [[ -n "$OLD_PREFIX_ARG" ]]; then
    PREFIX_ARG=$OLD_TESTS_ENVIRONMENT
    OLD_PREFIX_ARG=
  else
    PREFIX_ARG=
  fi
}

push_TESTS_ENVIRONMENT ()
{
  if [[ -n "$OLD_TESTS_ENVIRONMENT" ]]; then
    die "$LINENO: OLD_TESTS_ENVIRONMENT was set on push, programmer error!"
  fi

  if [[ -n "$TESTS_ENVIRONMENT" ]]; then
    OLD_TESTS_ENVIRONMENT=$TESTS_ENVIRONMENT
    TESTS_ENVIRONMENT=
  fi
}

pop_TESTS_ENVIRONMENT ()
{
  TESTS_ENVIRONMENT=
  if [[ -n "$OLD_TESTS_ENVIRONMENT" ]]; then
    TESTS_ENVIRONMENT=$OLD_TESTS_ENVIRONMENT
    OLD_TESTS_ENVIRONMENT=
  fi
}

safe_pushd ()
{
  pushd $1 &> /dev/null ;

  if $VERBOSE -a test -n "$BUILD_DIR"; then
    echo "BUILD_DIR=$BUILD_DIR"
  fi
}

safe_popd ()
{
  local directory_to_delete=`pwd`
  popd &> /dev/null ;
  if [ $? -eq 0 ]; then
    if [[ "$top_srcdir" == "$directory_to_delete" ]]; then
      die "$LINENO: We almost deleted top_srcdir($top_srcdir), programmer error"
    fi

    rm -r -f "$directory_to_delete"
  fi
}

make_valgrind ()
{
  if [[ "$VENDOR_DISTRIBUTION" == "darwin" ]]; then
    make_darwin_malloc
    return
  fi

  # If the env VALGRIND_COMMAND is set then we assume it is valid
  local valgrind_was_set=false
  if [[ -z "$VALGRIND_COMMAND" ]]; then
    setup_valgrind_command
    if [[ -n "$VALGRIND_COMMAND" ]]; then
      valgrind_was_set=true
    fi
  else
    valgrind_was_set=true
  fi

  # If valgrind_was_set is set to no we bail
  if ! $valgrind_was_set; then
    echo "valgrind was not present"
    return 1
  fi

  push_TESTS_ENVIRONMENT

  if [[ -f "libtool" ]]; then
    TESTS_ENVIRONMENT="$LIBTOOL_COMMAND $VALGRIND_COMMAND"
  else
    TESTS_ENVIRONMENT="$VALGRIND_COMMAND"
  fi

  make_target "check" || return 1

  pop_TESTS_ENVIRONMENT
}

make_install_system ()
{
  local INSTALL_LOCATION=$(mktemp -d /tmp/XXXXXXXXXX)
  push_PREFIX_ARG $INSTALL_LOCATION

  run_configure #install_buid_dir

  push_TESTS_ENVIRONMENT

  make_target "install"

  make_target "installcheck"

  make_target "uninstall"

  pop_TESTS_ENVIRONMENT
  pop_PREFIX_ARG

  rm -r -f $INSTALL_LOCATION
  make_maintainer_clean

  safe_popd
}

make_darwin_malloc ()
{
  old_MallocGuardEdges=$MallocGuardEdges
  MallocGuardEdges=1
  old_MallocErrorAbort=$MallocErrorAbort
  MallocErrorAbort=1
  old_MallocScribble=$MallocScribble
  MallocScribble=1

  make_check

  MallocGuardEdges= $old_MallocGuardEdges
  MallocErrorAbort= $old_MallocErrorAbort
  MallocScribble= $old_MallocScribble
}

make_for_continuus_integration ()
{
  # If this is really Jenkins everything will be clean, but if not...
  if [[ -z "$JENKINS_HOME" ]]; then
    make_maintainer_clean
  fi

  if [ -f 'Makefile' ]; then
    die "$LINENO: Programmer error, the file Makefile existed where build state should have been clean"
  fi

  if [ -f 'configure' ]; then
    die "$LINENO: Programmer error, the file configure existed where build state should have been clean"
  fi

  case $HOST_OS in
    *-fedora-*)
      if [[ "x$VENDOR_RELEASE" == "x17" ]]; then
        make_maintainer_clean
        run_autoreconf
      fi

      if [[ -f 'Makefile' ]]; then
        die "$LINENO: Programmer error, Makefile existed where build state should have been clean"
      fi

      run_configure

      # make rpm includes "make distcheck"
      if [[ -f rpm.am ]]; then
        make_rpm
      elif [[ -d rpm ]]; then
        make_rpm
      else
        make_distcheck
      fi
      make_install_system
      ;;
    *-precise-*)
      if [ "x$VENDOR_RELEASE" == 'precise' ]; then
        make_maintainer_clean
      fi

      if [[ -f 'Makefile' ]]; then
        die "$LINENO: Programmer error, Makefile existed where build state should have been clean"
      fi

      run_configure

      make_distcheck
      make_valgrind
      make_install_system
      ;;
    *)
      run_configure
      make_all
      ;;
  esac

  make_maintainer_clean

  safe_popd
}

make_gdb () {
  if command_exists gdb; then

    push_TESTS_ENVIRONMENT

    # Set ENV GDB_COMMAND
    if [[ -z "$GDB_COMMAND" ]]; then
      setup_gdb_command
    fi

    if [[ -f libtool ]]; then
      TESTS_ENVIRONMENT="$LIBTOOL_COMMAND $GDB_COMMAND"
    else
      TESTS_ENVIRONMENT="$GDB_COMMAND"
    fi

    make_target check

    if [[ -f gdb.txt ]]; then
      rm -f gdb.txt
    fi

    pop_TESTS_ENVIRONMENT

    if [[ -f '.gdb_history' ]]; then
      rm '.gdb_history'
    fi
  else
    echo "gdb was not present"
    return 1
  fi
}

# $1 target to compile
# $2 to die, or not to die, based on contents
make_target ()
{
  if [[ -z "$1" ]]; then
    die "$LINENO: Programmer error, no target provided for make"
  fi

  if [[ ! -f "Makefile" ]]; then
    die "$LINENO: Programmer error, make was called before configure"
    run_configure
  fi

  if test -n "$TESTS_ENVIRONMENT" -a $VERBOSE; then
    echo "TESTS_ENVIRONMENT=$TESTS_ENVIRONMENT"
  fi

  if [[ -z "$MAKE" ]]; then
    die "$LINENO: MAKE was not set"
  fi

  if [[ -n "$2" ]]; then
    run $MAKE $1 || return 1
  else
    run $MAKE $1 || die "$LINENO: Cannot execute $MAKE $1"
  fi
}

make_distcheck ()
{
  make_target 'distcheck'
}

make_rpm ()
{
  make_target 'rpm'
}

make_distclean ()
{
  make_target 'distclean'
}

make_maintainer_clean ()
{
  run_configure_if_required
  make_target 'maintainer-clean' 'no_error'
}

make_check ()
{
  make_target 'check'
}

make_all ()
{
  make_target 'all'
}

run_configure_if_required () 
{
  run_autoreconf_if_required

  if [ ! -f 'Makefile' ]; then
    run_configure
  fi
}

run_autoreconf_if_required () 
{
  if [ ! -x 'configure' ]; then
    run_autoreconf
  fi
}

run_autoreconf () 
{
  if [[ -z "$AUTORECONF" ]]; then
    die "$LINENO: Programmer error, tried to call run_autoreconf () but AUTORECONF was not set"
  fi

  run $AUTORECONF || die "$LINENO: Cannot execute $AUTORECONF"
}

run ()
{
  if $VERBOSE; then
    echo "\`$@' $ARGS"
  fi

  $@ $ARGS
} 

parse_command_line_options ()
{
  local options=

  local SHORTOPTS='p,c,a,v'
  local LONGOPTS='target:,debug,clean,print-env,configure,autoreconf' 

  if ! options=$(getopt --long target: --long debug --long clean -o p --long print-env -o c --long configure -o a --long autoreconf -n 'bootstrap' -- "$@"); then
    die 'Bad option given'
  fi

  eval set -- "$options"

  while [[ $# -gt 0 ]]; do
    case $1 in
      -a | --autoreconf )
        AUTORECONF_OPTION=true ; MAKE_TARGET='autoreconf' ; shift;;
      -p | --print-env )
        PRINT_SETUP_OPTION=true ; shift;;
      -c | --configure )
        CONFIGURE_OPTION=true ; MAKE_TARGET='configure' ; shift;;
      --clean )
        CLEAN_OPTION=true ; MAKE_TARGET='clean_op' ; shift;;
      --target )
        TARGET_OPTION=true ; shift; MAKE_TARGET="$1" ; shift;;
      --debug )
        DEBUG_OPTION=true ; DEBUG=true ; shift;;
      -v | --verbose )
        VERBOSE_OPTION=true ; VERBOSE=true ; shift;;
      -- )
        shift; break;;
      -* )
        echo "$0: error - unrecognized option $1" 1>&2; exit 1;;
      *)
        break;;
    esac
  done

  if [ -n "$1" ]; then
    MAKE_TARGET="$1"
  fi
}

determine_vcs ()
{
  if [[ -d '.git' ]]; then
    VCS_CHECKOUT=git
  elif [[ -d '.bzr' ]]; then
    VCS_CHECKOUT=bzr
  elif [[ -d '.svn' ]]; then
    VCS_CHECKOUT=svn
  elif [[ -d '.hg' ]]; then
    VCS_CHECKOUT=hg
  fi

  if [[ -n "$VCS_CHECKOUT" ]]; then
    VERBOSE=true
  fi
}

autoreconf_setup ()
{
  # Set ENV MAKE in order to override "make"
  if [[ -z "$MAKE" ]]; then 
    if command_exists gmake; then
      MAKE=`type -p gmake`
    fi

    if command_exists make; then
      MAKE=`type -p make`
    fi
  fi

  if [[ -z "$GNU_BUILD_FLAGS" ]]; then
    GNU_BUILD_FLAGS="--install --force"
  fi

  if $VERBOSE; then
    GNU_BUILD_FLAGS="$GNU_BUILD_FLAGS --verbose"
  fi

  if [[ -z "$WARNINGS" ]]; then
    if [[ -n "$VCS_CHECKOUT" ]]; then
      WARNINGS="all,error"
    else
      WARNINGS="all"
    fi
  fi

  if [[ -z "$LIBTOOLIZE" ]]; then
    # If we are using OSX, we first check to see glibtoolize is available
    if [[ "$VENDOR_DISTRIBUTION" == "darwin" ]]; then
      LIBTOOLIZE=`type -p glibtoolize`

      if [[ -z "$LIBTOOLIZE" ]]; then
        echo "Couldn't find glibtoolize, it is required on OSX"
      fi
    fi
  fi

  # Test the ENV AUTOMAKE if it exists
  if [[ -n "$AUTOMAKE" ]]; then
    run $AUTOMAKE '--help'    &> /dev/null    || die "$LINENO: Failed to run AUTOMAKE:$AUTOMAKE"
  fi

  # Test the ENV AUTOCONF if it exists
  if [[ -n "$AUTOCONF" ]]; then
    run $AUTOCONF '--help'    &> /dev/null    || die "$LINENO: Failed to run AUTOCONF:$AUTOCONF"
  fi

  # Test the ENV AUTOHEADER if it exists
  if [[ -n "$AUTOHEADER" ]]; then
    run $AUTOHEADER '--help'  &> /dev/null    || die "$LINENO: Failed to run AUTOHEADER:$AUTOHEADER"
  fi

  # Test the ENV AUTOM4TE if it exists
  if [[ -n "$AUTOM4TE" ]]; then
    run $AUTOM4TE '--help'    &> /dev/null    || die "$LINENO: Failed to run AUTOM4TE:$AUTOM4TE"
  fi

  if [[ -z "$AUTORECONF" ]]; then
    AUTORECONF=`type -p autoreconf`

    if [[ -z "$AUTORECONF" ]]; then
      die "Couldn't find autoreconf"
    fi

    if [[ -n "$GNU_BUILD_FLAGS" ]]; then
      AUTORECONF="$AUTORECONF $GNU_BUILD_FLAGS"
    fi
  fi

  run $AUTORECONF '--help'  &> /dev/null    || die "$LINENO: Failed to run AUTORECONF:$AUTORECONF"
}

print_setup ()
{
  echo 
  echo "AUTORECONF=$AUTORECONF"
  echo "HOST_OS=$HOST_OS"

  if [[ -n "$MAKE" ]]; then
    echo "MAKE=$MAKE"
  fi

  if [[ -n "$MAKE_TARGET" ]]; then
    echo "MAKE_TARGET=$MAKE_TARGET"
  fi

  if [[ -n "$PREFIX" ]]; then
    echo "PREFIX=$PREFIX"
  fi

  if [[ -n "$TESTS_ENVIRONMENT" ]]; then
    echo "TESTS_ENVIRONMENT=$TESTS_ENVIRONMENT"
  fi

  if [[ -n "$VCS_CHECKOUT" ]]; then
    echo "VCS_CHECKOUT=$VCS_CHECKOUT"
  fi

  if $VERBOSE; then
    echo "VERBOSE=true"
  fi

  if [[ -n "$WARNINGS" ]]; then
    echo "WARNINGS=$WARNINGS"
  fi
}

make_clean_option ()
{
  run_configure_if_required

  make_maintainer_clean

  if [[ "$VCS_CHECKOUT" == 'git' ]]; then
    run "$VCS_CHECKOUT" status --ignored
  elif [[ -n "$VCS_CHECKOUT" ]]; then
    run "$VCS_CHECKOUT" status
  fi
}

bootstrap ()
{
  determine_target_platform

  determine_vcs

  # Set up whatever we need to do to use autoreconf later
  autoreconf_setup

  if $PRINT_SETUP_OPTION -o  $PRINT_ENV_DEBUG_OPTION; then
    echo 
    print_setup
    echo 

    # Exit if all we were looking for were the currently used options
    if $PRINT_SETUP_OPTION; then
      exit
    fi
  fi

  # Setup LIBTOOL_COMMAND if we need it
  if [[ -f "libtool" ]]; then
    LIBTOOL_COMMAND='./libtool --mode=execute'
  fi

  # Use OLD_TESTS_ENVIRONMENT for tracking the state of the variable
  local OLD_TESTS_ENVIRONMENT=

  # Set ENV PREFIX in order to set --prefix for ./configure
  if [[ -n "$PREFIX" ]]; then 
    push_PREFIX_ARG $PREFIX
  fi

  # If we are running under Jenkins we predetermine what tests we will run against
  if [[ -n "$JENKINS_HOME" ]]; then 
    MAKE_TARGET='jenkins'
  fi

  if [[ "$MAKE_TARGET" == 'gdb' ]]; then
    run_configure_if_required
    make_gdb || die "$LINENO: gdb was not found"
  elif [[ "$MAKE_TARGET" == 'clean_op' ]]; then
    make_clean_option
    return
  elif [[ "$MAKE_TARGET" == 'autoreconf' ]]; then
    run_autoreconf
    return
  elif [[ "$MAKE_TARGET" == 'configure' ]]; then
    run_configure
    return
  elif [[ "$MAKE_TARGET" == 'valgrind' ]]; then
    run_configure_if_required
    make_valgrind || die "$LINENO: valrind was not found"
  elif [[ "$MAKE_TARGET" == 'jenkins' ]]; then 
    make_for_continuus_integration
  elif [[ -z "$MAKE_TARGET" ]]; then 
    run_configure_if_required
    make_all
  else
    run_configure_if_required
    make_target $MAKE_TARGET
  fi
}

main ()
{
  if [[ -f '.bootstrap' ]]; then
    source '.bootstrap'
  fi

  # Variables we export
  declare -x VCS_CHECKOUT

  # Options for getopt
  local PRINT_ENV_DEBUG_OPTION=false
  local PRINT_SETUP_OPTION=false

  local AUTORECONF_OPTION=false
  local CLEAN_OPTION=false
  local CONFIGURE_OPTION=false
  local DEBUG_OPTION=false
  local TARGET_OPTION="$MAKE_TARGET"
  local VERBOSE_OPTION=false

  # If we call autoreconf on the platform or not
  local AUTORECONF_REBUILD_HOST=false
  local AUTORECONF_REBUILD=false

  local -r top_srcdir=`pwd`

  # Variables for determine_target_platform () and rebuild_host_os ()
  #   UNAME_MACHINE_ARCH= uname -m
  #   VENDOR= apple, redhat, centos, canonical
  #   VENDOR_RELEASE=  
  #                  RHEL{rhel,Tikanga,Santiago}
  #                  Ubuntu{ubuntu,Lucid,Maverick,Natty,Oneiric,Precise,Quantal}
  #                  Fedora{fedora,Verne,Beefy}
  #                  OSX{osx,lion,snow,mountain}
  #   VENDOR_DISTRIBUTION= darwin,fedora,rhel,ubuntu
  #   UNAME_KERNEL= Linux, Darwin,...
  #   UNAME_KERNEL_RELEASE= Linux, Darwin,...
  local UNAME_MACHINE_ARCH=unknown
  local VENDOR=unknown
  local VENDOR_RELEASE=unknown
  local VENDOR_DISTRIBUTION=unknown
  local UNAME_KERNEL=unknown
  local UNAME_KERNEL_RELEASE=unknown
  local HOST_OS=

  rebuild_host_os no_output

  parse_command_line_options $@

  bootstrap
  jobs -l
  wait

  exit 0
}

export AUTOCONF
export AUTOHEADER
export AUTOM4TE
export AUTOMAKE
export AUTORECONF
export DEBUG
export GNU_BUILD_FLAGS
export MAKE
export TESTS_ENVIRONMENT
export VERBOSE
export WARNINGS

if [[ -n "$VERBOSE" ]]; then
  VERBOSE=true
else
  VERBOSE=false
fi

if [[ -n "$DEBUG" ]]; then
  DEBUG=true
else
  DEBUG=false
fi

case $OSTYPE in
  darwin*)
    export MallocGuardEdges
    export MallocErrorAbort
    export MallocScribble
    ;;
esac

if [[ -f '.bootstrap' ]]; then
  source '.bootstrap'
fi

main $@
