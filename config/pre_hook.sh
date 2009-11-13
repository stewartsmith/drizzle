echo "Running register_plugins.py"
if test ! -f  config/plugin.stamp
then
  touch config/plugin.stamp
fi
python config/register_plugins.py plugin.list
python config/register_plugins.py plugin-list.am
python config/register_plugins.py plugin-list.ac
python config/register_plugins.py . .
