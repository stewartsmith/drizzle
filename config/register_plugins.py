#!/usr/bin/python

#  Copyright (C) 2009 Sun Microsystems
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


# Find plugins in the tree and add them to the build system 

import ConfigParser, os, sys

top_srcdir='.'
top_builddir='.'
plugin_ini_fname='plugin.ini'
plugin_list=[]

class ChangeProtectedFile(object):

  def __init__(self, fname):
    self.bogus_file= False
    self.real_fname= fname
    self.new_fname= "%s.new" % fname
    try:
      self.new_file= open(self.new_fname,'w+')
    except IOError:
      self.bogus_file= True

  def write(self, text):
    if not self.bogus_file:
      self.new_file.write(text)

  # We've written all of this out into .new files, now we only copy them
  # over the old ones if they are different, so that we don't cause 
  # unnecessary recompiles
  def close(self):
    """Return True if the file had changed."""
    if self.bogus_file:
      return
    self.new_file.seek(0)
    new_content = self.new_file.read()
    self.new_file.close()
    try:
        old_file = file(self.real_fname, 'r')
        old_content = old_file.read()
        old_file.close()
    except IOError:
        old_content = None
    if new_content != old_content:
      if old_content != None:
        os.unlink(self.real_fname)
      os.rename(self.new_fname, self.real_fname)
      return True
    else:
        try:
          os.unlink(self.new_fname)
        except:
          pass


def write_external_plugin():
  """Return True if the plugin had changed."""
  plugin = read_plugin_ini('.')
  expand_plugin_ini(plugin, '.')
  plugin_file = ChangeProtectedFile('pandora-plugin.ac')
  write_plugin_ac(plugin, plugin_file)
  result = plugin_file.close()
  plugin_file = ChangeProtectedFile('pandora-plugin.am')
  write_plugin_am(plugin, plugin_file)
  # Write some stub configure.ac and Makefile.am files that include the above
  result = plugin_file.close() or result
  return result

def write_plugin(plugin_dir):
  """Return True if the plugin had changed."""
  plugin = read_plugin_ini(plugin_dir)
  expand_plugin_ini(plugin, plugin_dir)
  plugin_file = ChangeProtectedFile(os.path.join(plugin_dir, 'pandora-plugin.ac'))
  write_plugin_ac(plugin, plugin_file)
  result = plugin_file.close()
  plugin_file = ChangeProtectedFile(os.path.join(plugin_dir, 'pandora-plugin.am'))
  write_plugin_am(plugin, plugin_file)
  result = plugin_file.close() or result
  return result

def write_plugin_ac(plugin, plugin_ac):
  #
  # Write plugin config instructions into plugin.ac file.
  #
  plugin_ac_file=os.path.join(plugin['rel_path'],'plugin.ac')
  plugin_m4_dir=os.path.join(plugin['rel_path'],'m4')
  plugin_m4_files=[]
  if os.path.exists(plugin_m4_dir) and os.path.isdir(plugin_m4_dir):
    for m4_file in os.listdir(plugin_m4_dir):
      if os.path.splitext(m4_file)[-1] == '.m4':
        plugin_m4_files.append(os.path.join(plugin['rel_path'], m4_file))
  plugin_ac.write("""dnl Generated file, run make to rebuild
dnl Config for %(title)s
""" % plugin)
  for m4_file in plugin_m4_files:
    plugin_ac.write('m4_sinclude([%s])\n' % m4_file)

  plugin_ac.write("""
AC_ARG_WITH([%(name_with_dashes)s-plugin],[
dnl indented werid to make the help output correct
AS_HELP_STRING([--with-%(name_with_dashes)s-plugin],[Build %(title)s and enable it. @<:@default=%(default_yesno)s@:>@])
AS_HELP_STRING([--without-%(name_with_dashes)s-plugin],[Disable building %(title)s])
  ],
  [with_%(name)s_plugin="$withval"],
  [AS_IF([test "x$ac_with_all_plugins" = "xyes"],
         [with_%(name)s_plugin=yes],
         [with_%(name)s_plugin=%(default_yesno)s])])
""" % plugin)
  if os.path.exists(plugin_ac_file):
    plugin_ac.write('m4_sinclude([%s])\n' % plugin_ac_file) 
  # The plugin author has specified some check to make to determine
  # if the plugin can be built. If the plugin is turned on and this 
  # check fails, then configure should error out. If the plugin is not
  # turned on, then the normal conditional build stuff should just let
  # it silently not build
  if plugin['has_build_conditional']:
    plugin_ac.write("""
AS_IF([test %(build_conditional)s],
      [], dnl build_conditional can only negate
      [with_%(name)s_plugin=no])
  """ % plugin)
  plugin['plugin_dep_libs']=" ".join(["\${top_builddir}/%s" % f for f in plugin['libs'].split()])
  plugin_ac.write("""
AM_CONDITIONAL([%(build_conditional_tag)s],
               [test %(build_conditional)s])
AS_IF([test "x$with_%(name)s_plugin" = "xyes"],
      [
  """ % plugin)
  if plugin['testsuite']:
    plugin_ac.write("""
        pandora_plugin_test_list="%(name)s,${pandora_plugin_test_list}"
    """ % plugin)

  if plugin['static']:
    #pandora_default_plugin_list="%(name)s,${pandora_default_plugin_list}"
    plugin_ac.write("""
        pandora_builtin_list="builtin_%(name)s_plugin,${pandora_builtin_list}"
        pandora_plugin_libs="${pandora_plugin_libs} \${top_builddir}/plugin/lib%(name)s_plugin.la"
	PANDORA_PLUGIN_DEP_LIBS="${PANDORA_PLUGIN_DEP_LIBS} %(plugin_dep_libs)s"
""" % plugin)
  else:
    plugin_ac.write("""
        pandora_default_plugin_list="%(name)s,${pandora_default_plugin_list}"
    """ % plugin)
  plugin_ac.write("      ])\n")


def expand_plugin_ini(plugin, plugin_dir):
    plugin['rel_path']= plugin_dir[len(top_srcdir)+len(os.path.sep):]
    # TODO: determine path to plugin dir relative to top_srcdir... append it to
    # source files if they don't already have it
    if plugin['sources'] == "":
      plugin['sources']="%s.cc" % plugin['name']
    new_sources=""
    for src in plugin['sources'].split():
      if not src.startswith(plugin['rel_path']):
        src= os.path.join(plugin['rel_path'], src)
        new_sources= "%s %s" % (new_sources, src)
    plugin['sources']= new_sources
    
    new_headers=""
    for header in plugin['headers'].split():
      if not header.startswith(plugin['rel_path']):
        header= os.path.join(plugin['rel_path'], header)
        new_headers= "%s %s" % (new_headers, header)
    plugin['headers']= new_headers
    
    # Make a yes/no version for autoconf help messages
    if plugin['load_by_default']:
      plugin['default_yesno']="yes"
    else:
      plugin['default_yesno']="no"

    
    plugin['build_conditional_tag']= "BUILD_%s_PLUGIN" % plugin['name'].upper()
    plugin['name_with_dashes']= plugin['name'].replace('_','-')
    if plugin.has_key('build_conditional'):
      plugin['has_build_conditional']=True
    else:
      plugin['has_build_conditional']=False
      plugin['build_conditional']='"x${with_%(name)s_plugin}" = "xyes"' %plugin

def find_testsuite(plugin_dir):
  for testdir in ['drizzle-tests','tests']:
    if os.path.isdir(os.path.join(plugin_dir,testdir)):
      return testdir
  if os.path.isdir(os.path.join('tests','suite',os.path.basename(plugin_dir))):
    return ""
  return None

def read_plugin_ini(plugin_dir):
    plugin_file= os.path.join(plugin_dir,plugin_ini_fname)
    parser=ConfigParser.ConfigParser(defaults=dict(sources="",headers="", cflags="",cppflags="",cxxflags="", libs="", ldflags=""))
    parser.read(plugin_file)
    plugin=dict(parser.items('plugin'))
    plugin['name']= os.path.basename(plugin_dir)
    if plugin.has_key('load_by_default'):
      plugin['load_by_default']=parser.getboolean('plugin','load_by_default')
    else:
      plugin['load_by_default']=False
    if plugin.has_key('static'):
      plugin['static']= parser.getboolean('plugin','static')
    else:
      plugin['static']= False
    if plugin.has_key('testsuite'):
      if plugin['testsuite'] == 'disable':
        plugin['testsuite']= False
    else:
      plugin_testsuite= find_testsuite(plugin_dir)
      plugin['testsuitedir']=plugin_testsuite
      if plugin_testsuite is not None:
        plugin['testsuite']=True
      else:
        plugin['testsuite']=False

    return plugin


def write_plugin_am(plugin, plugin_am):
  """Write an automake fragment for this plugin.
  
  :param plugin: The plugin dict.
  :param plugin_am: The file to write to.
  """
  # The .plugin.ini.stamp avoids changing the datestamp on plugin.ini which can
  # confuse VCS systems.
  plugin_am.write("""## Generated by register_plugins.py
EXTRA_DIST += %(rel_path)s/pandora-plugin.ac %(rel_path)s/.plugin.ini.stamp

${top_srcdir}/%(rel_path)s/pandora-plugin.am: ${top_srcdir}/config/register_plugins.py %(rel_path)s/.plugin.ini.stamp

${top_srcdir}/%(rel_path)s/pandora-plugin.ac: ${top_srcdir}/config/register_plugins.py %(rel_path)s/.plugin.ini.stamp

configure: ${top_srcdir}/%(rel_path)s/pandora-plugin.ac

# Prevent errors when a plugin dir is removed
%(rel_path)s/plugin.ini:

# Failures to update the plugin.ini are ignored to permit plugins to be deleted
# cleanly.
%(rel_path)s/.plugin.ini.stamp: %(rel_path)s/plugin.ini
	@if [ ! -e ${top_srcdir}/%(rel_path)s/plugin.ini ]; then \\
	    echo "%(rel_path)s/plugin.ini is missing"; \\
	else \\
	    cmp -s $< $@; \\
	    if [ $$? -ne 0 ]; then \\
	      echo 'cd ${srcdir} && python config/register_plugins.py . . %(rel_path)s'; \\
	      unchanged=`cd ${srcdir} && python config/register_plugins.py . . %(rel_path)s` ;\\
	      if [ $$? -ne 0 ]; then \\
	        echo "**** register_plugins failed ****"; \\
	        false; \\
	      fi && \\
	      for plugin_dir in $$unchanged; do \\
	        echo "plugin $$plugin_dir unchanged." ; \\
	        touch -f -r ${top_srcdir}/$$plugin_dir/pandora-plugin.am $$plugin_dir/.plugin.ini.stamp; \\
	      done && \\
              cp $< $@ || echo "Failed to update $@"; \\
	    fi; \\
	fi
""" % plugin)
  if plugin['headers'] != "":
    plugin_am.write("noinst_HEADERS += %(headers)s\n" % plugin)
  if plugin['testsuite']:
    if plugin.has_key('testsuitedir') and plugin['testsuitedir'] != "":
      plugin_am.write("EXTRA_DIST += %(rel_path)s/%(testsuitedir)s\n" % plugin)
  if plugin['static']:
    plugin_am.write("""
plugin_lib%(name)s_dir=${top_srcdir}/%(rel_path)s
EXTRA_DIST += %(rel_path)s/plugin.ini
if %(build_conditional_tag)s
  noinst_LTLIBRARIES+=plugin/lib%(name)s_plugin.la
  plugin_lib%(name)s_plugin_la_LIBADD=%(libs)s
  plugin_lib%(name)s_plugin_la_DEPENDENCIES=%(libs)s
  plugin_lib%(name)s_plugin_la_LDFLAGS=$(AM_LDFLAGS) %(ldflags)s
  plugin_lib%(name)s_plugin_la_CPPFLAGS=$(AM_CPPFLAGS) -DPANDORA_MODULE_NAME=%(name)s %(cppflags)s
  plugin_lib%(name)s_plugin_la_CXXFLAGS=$(AM_CXXFLAGS) %(cxxflags)s
  plugin_lib%(name)s_plugin_la_CFLAGS=$(AM_CFLAGS) %(cflags)s

  plugin_lib%(name)s_plugin_la_SOURCES=%(sources)s
  PANDORA_DYNAMIC_LDADDS+=${top_builddir}/plugin/lib%(name)s_plugin.la
endif
""" % plugin)
  else:
    plugin_am.write("""
plugin_lib%(name)s_dir=${top_srcdir}/%(rel_path)s
EXTRA_DIST += %(rel_path)s/plugin.ini
if %(build_conditional_tag)s
  pkgplugin_LTLIBRARIES+=plugin/lib%(name)s_plugin.la
  plugin_lib%(name)s_plugin_la_LDFLAGS=-module -avoid-version -rpath $(pkgplugindir) $(AM_LDFLAGS) %(ldflags)s
  plugin_lib%(name)s_plugin_la_LIBADD=%(libs)s
  plugin_lib%(name)s_plugin_la_DEPENDENCIES=%(libs)s
  plugin_lib%(name)s_plugin_la_CPPFLAGS=$(AM_CPPFLAGS) -DPANDORA_DYNAMIC_PLUGIN -DPANDORA_MODULE_NAME=%(name)s %(cppflags)s
  plugin_lib%(name)s_plugin_la_CXXFLAGS=$(AM_CXXFLAGS) %(cxxflags)s
  plugin_lib%(name)s_plugin_la_CFLAGS=$(AM_CFLAGS) %(cflags)s

  plugin_lib%(name)s_plugin_la_SOURCES=%(sources)s
endif
""" % plugin)
  plugin_am_file=os.path.join(plugin['rel_path'],'plugin.am')
  if os.path.exists(plugin_am_file):
    plugin_am.write('include %s\n' % plugin_am_file)

if len(sys.argv)>2:
  top_srcdir=sys.argv[1]
  top_builddir=sys.argv[2]

if not os.path.exists("config/plugin.list"):
  os.system("find plugin -name 'plugin.ini' | xargs -n1 dirname | xargs -n1 basename | sort -b -d > .plugin.scan")
  os.system("cp .plugin.scan config/plugin.list")


if len(sys.argv)>2 or (len(sys.argv)==2 and sys.argv[1]=='plugin-stamp'):
  if len(sys.argv)>3:
    plugin_list = [top_srcdir + '/' + plugin_name for plugin_name in sys.argv[3:]]
  else:
    def accumulate_plugins(arg, dirname, fnames):
      # plugin_ini_fname is a name in dirname indicating dirname is a plugin.
      if plugin_ini_fname in fnames:
        arg.append(dirname)
    os.path.walk(os.path.join(top_srcdir,"plugin"),accumulate_plugins,plugin_list)

if os.path.exists('plugin.ini'):
  # Are we in a plugin dir which wants to have a self-sufficient build system?
  write_external_plugin()
else:
  plugin_list.sort()
  for plugin_dir in plugin_list:
    if not write_plugin(plugin_dir):
      if not (len(sys.argv)==2 and sys.argv[1]=='plugin-stamp'):
        print plugin_dir

if not os.path.exists("config/plugin-list.am") or (len(sys.argv)==2 and sys.argv[0]=='plugin-list.am'):
  os.system(r"sed 's,^\(.*\)$,include plugin/\1/pandora-plugin.am,' < config/plugin.list > config/plugin-list.am")

if not os.path.exists("config/plugin-list.ac") or (len(sys.argv)==2 and sys.argv[0]=='plugin-list.ac'):
  os.system(r"sed 's,^\(.*\)$,m4_sinclude(plugin/\1/pandora-plugin.ac),'  < config/plugin.list > config/plugin-list.ac")
