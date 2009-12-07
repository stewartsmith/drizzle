if test ! -f  config/plugin.stamp
then
  touch config/plugin.stamp
fi
run python config/pandora-plugin write
