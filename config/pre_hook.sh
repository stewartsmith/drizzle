echo "Running register_plugins.py"
if test ! -f  config/plugin.stamp
then
  touch config/plugin.stamp
fi
python config/register_plugins.py write
