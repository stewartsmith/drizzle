# pre_hook.sh - Commands that we run before we run the autotools


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
  if diff po/POTFILES.in.in po/POTFILES.in >/dev/null 2>&1
  then
    rm po/POTFILES.in.in
  else
    mv po/POTFILES.in.in po/POTFILES.in
  fi
else
  touch po/POTFILES.in
fi

run python config/register_plugins.py || die  "Can't execute register_plugins"
