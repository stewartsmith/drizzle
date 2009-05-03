#!/usr/bin/env bash
# Taken from lighthttpd server (BSD). Thanks Jan!
# Run this to generate all the initial makefiles, etc.

die() { echo "$@"; exit 1; }

# LIBTOOLIZE=${LIBTOOLIZE:-libtoolize}
LIBTOOLIZE_FLAGS=" --automake --copy --force"
# ACLOCAL=${ACLOCAL:-aclocal}
ACLOCAL_FLAGS="-I m4"
# AUTOHEADER=${AUTOHEADER:-autoheader}
# AUTOMAKE=${AUTOMAKE:-automake}
# --add-missing instructs automake to install missing auxiliary files
# --copy tells it to make copies and not symlinks
AUTOMAKE_FLAGS="--add-missing --copy --force"
# AUTOCONF=${AUTOCONF:-autoconf}

ARGV0=$0
ARGS="$@"


run() {
	echo "$ARGV0: running \`$@' $ARGS"
	$@ $ARGS
}

## jump out if one of the programs returns 'false'
set -e

if test -d ".bzr" ; then
  bzr log --short > ChangeLog || touch ChangeLog
  BZR_REVNO=`bzr revno`
  BZR_REVID=`bzr log -r-1 --show-ids | grep revision-id | awk '{print $2}' | head -1`
  BZR_BRANCH=`bzr nick`
  RELEASE_DATE=`date +%Y.%m`
  RELEASE_VERSION="${RELEASE_DATE}.${BZR_REVNO}"
  if test "x${BZR_BRANCH}" != "xdrizzle" ; then
    RELEASE_VERSION="${RELEASE_VERSION}-${BZR_BRANCH}"
  fi
else
  BZR_REVNO="0"
  BZR_REVID="unknown"
  BZR_BRANCH="bzr-export"
  RELEASE_DATE=`date +%Y.%m`
  RELEASE_VERSION="${RELEASE_DATE}.${BZR_REVNO}"
  touch ChangeLog
fi
sed -e "s/@BZR_REVNO@/${BZR_REVNO}/" \
    -e "s/@BZR_REVID@/${BZR_REVID}/" \
    -e "s/@BZR_BRANCH@/${BZR_BRANCH}/" \
    -e "s/@RELEASE_DATE@/${RELEASE_DATE}/" \
    -e "s/@RELEASE_VERSION@/${RELEASE_VERSION}/" \
  m4/bzr_version.m4.in > m4/bzr_version.m4.new

if ! diff m4/bzr_version.m4.new m4/bzr_version.m4 >/dev/null 2>&1 ; then
  mv m4/bzr_version.m4.new m4/bzr_version.m4
else
  rm m4/bzr_version.m4.new
fi

python config/register_plugins.py

if test x$LIBTOOLIZE = x; then
  if test \! "x`which glibtoolize 2> /dev/null | grep -v '^no'`" = x; then
    LIBTOOLIZE=glibtoolize
  elif test \! "x`which libtoolize-1.5 2> /dev/null | grep -v '^no'`" = x; then
    LIBTOOLIZE=libtoolize-1.5
  elif test \! "x`which libtoolize 2> /dev/null | grep -v '^no'`" = x; then
    LIBTOOLIZE=libtoolize
  elif test \! "x`which glibtoolize 2> /dev/null | grep -v '^no'`" = x; then
    LIBTOOLIZE=glibtoolize
  else
    echo "libtoolize 1.5.x wasn't found, exiting"; exit 1
  fi
fi

if test x$ACLOCAL = x; then
  if test \! "x`which aclocal-1.10 2> /dev/null | grep -v '^no'`" = x; then
    ACLOCAL=aclocal-1.10
  elif test \! "x`which aclocal110 2> /dev/null | grep -v '^no'`" = x; then
    ACLOCAL=aclocal110
  elif test \! "x`which aclocal 2> /dev/null | grep -v '^no'`" = x; then
    ACLOCAL=aclocal
  else 
    echo "automake 1.10.x (aclocal) wasn't found, exiting"; exit 1
  fi
fi

if test x$AUTOMAKE = x; then
  if test \! "x`which automake-1.10 2> /dev/null | grep -v '^no'`" = x; then
    AUTOMAKE=automake-1.10
  elif test \! "x`which automake110 2> /dev/null | grep -v '^no'`" = x; then
    AUTOMAKE=automake110
  elif test \! "x`which automake 2> /dev/null | grep -v '^no'`" = x; then
    AUTOMAKE=automake
  else 
    echo "automake 1.10.x wasn't found, exiting"; exit 1
  fi
fi


## macosx has autoconf-2.59 and autoconf-2.60
if test x$AUTOCONF = x; then
  if test \! "x`which autoconf-2.59 2> /dev/null | grep -v '^no'`" = x; then
    AUTOCONF=autoconf-2.59
  elif test \! "x`which autoconf259 2> /dev/null | grep -v '^no'`" = x; then
    AUTOCONF=autoconf259
  elif test \! "x`which autoconf 2> /dev/null | grep -v '^no'`" = x; then
    AUTOCONF=autoconf
  else 
    echo "autoconf 2.59+ wasn't found, exiting"; exit 1
  fi
fi

if test x$AUTOHEADER = x; then
  if test \! "x`which autoheader-2.59 2> /dev/null | grep -v '^no'`" = x; then
    AUTOHEADER=autoheader-2.59
  elif test \! "x`which autoheader259 2> /dev/null | grep -v '^no'`" = x; then
    AUTOHEADER=autoheader259
  elif test \! "x`which autoheader 2> /dev/null | grep -v '^no'`" = x; then
    AUTOHEADER=autoheader
  else 
    echo "autoconf 2.59+ (autoheader) wasn't found, exiting"; exit 1
  fi
fi


# --force means overwrite ltmain.sh script if it already exists 
run $LIBTOOLIZE $LIBTOOLIZE_FLAGS || die "Can't execute libtoolize"

run $ACLOCAL $ACLOCAL_FLAGS || die "Can't execute aclocal"
run $AUTOHEADER || die "Can't execute autoheader"
run $AUTOMAKE $AUTOMAKE_FLAGS  || die "Can't execute automake"
run $AUTOCONF || die "Can't execute autoconf"
echo -n "Automade with: "
$AUTOMAKE --version | head -1
echo -n "Configured with: "
$AUTOCONF --version | head -1
