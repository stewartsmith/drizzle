#!/bin/sh
# pre_hook.sh - Commands that we run before we run the autotools

RELEASE_DATE=`date +%Y.%m`
RELEASE_DATE_NODOTS=`date +%Y%m`
if test -d ".bzr" ; then
  echo "Grabbing changelog and version information from bzr"
  bzr log --short > ChangeLog || touch ChangeLog
  BZR_REVNO=`bzr revno`
  BZR_REVID=`bzr log -r-1 --show-ids | grep revision-id | awk '{print $2}' | head -1`
  BZR_BRANCH=`bzr nick`
else
  touch ChangeLog
  BZR_REVNO="0"
  BZR_REVID="unknown"
  BZR_BRANCH="bzr-export"
fi
RELEASE_VERSION="${RELEASE_DATE}.${BZR_REVNO}"
RELEASE_ID="${RELEASE_DATE_NODOTS}${BZR_REVNO}"
if test "x${BZR_BRANCH}" != "xdrizzle" ; then
  RELEASE_COMMENT="${BZR_BRANCH}"
else
  RELEASE_COMMENT="trunk"
fi

if test -f m4/bzr_version.m4.in
then
  sed -e "s/@BZR_REVNO@/${BZR_REVNO}/" \
      -e "s/@BZR_REVID@/${BZR_REVID}/" \
      -e "s/@BZR_BRANCH@/${BZR_BRANCH}/" \
      -e "s/@RELEASE_DATE@/${RELEASE_DATE}/" \
      -e "s/@RELEASE_ID@/${RELEASE_ID}/" \
      -e "s/@RELEASE_VERSION@/${RELEASE_VERSION}/" \
      -e "s/@RELEASE_COMMENT@/${RELEASE_COMMENT}/" \
    m4/bzr_version.m4.in > m4/bzr_version.m4.new
  
  if ! diff m4/bzr_version.m4.new m4/bzr_version.m4 >/dev/null 2>&1 ; then
    mv m4/bzr_version.m4.new m4/bzr_version.m4
  else
    rm m4/bzr_version.m4.new
  fi
fi

EGREP=`which egrep`
if test "x$EGREP" != "x" -a -d po
then
  echo "# This file is auto-generated from configure. Do not edit directly" > po/POTFILES.in.in
  # The grep -v 'drizzle-' is to exclude any distcheck leftovers
  for f in `find . | grep -v 'drizzle-' | ${EGREP} '\.(cc|c|h|yy)$' | cut -c3- | sort`
  do
    if grep gettext.h "$f" | grep include >/dev/null 2>&1
    then
      echo "$f" >> po/POTFILES.in.in
    fi
  done
  if ! diff po/POTFILES.in.in po/POTFILES.in >/dev/null 2>&1
  then
    mv po/POTFILES.in.in po/POTFILES.in
  else
    rm po/POTFILES.in.in
  fi
else
  touch po/POTFILES.in
fi

run python config/register_plugins.py || die  "Can't execute register_plugins"
