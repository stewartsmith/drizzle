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

import os, ConfigParser, sys

top_srcdir='.'
top_builddir='.'
plugin_ini_fname='plugin.ini'
plugin_list=[]
autogen_header="This file is generated, re-run %s to rebuild\n" % sys.argv[0]

if len(sys.argv)>1:
  top_srcdir=sys.argv[1]
  top_builddir=top_srcdir
if len(sys.argv)>2:
  top_builddir=sys.argv[2]

plugin_ac=open(os.path.join(top_builddir,'config','plugin.ac.new'),'w')
plugin_am=open(os.path.join(top_srcdir,'config','plugin.am.new'),'w')
plugin_ac.write("dnl %s" % autogen_header)
plugin_am.write("# %s" % autogen_header)

def accumulate_plugins(arg, dirname, fnames):
  if plugin_ini_fname in fnames:
    arg.append(dirname)


os.path.walk(os.path.join(top_srcdir,"plugin"),accumulate_plugins,plugin_list)

for plugin_dir in plugin_list:
  plugin_file= os.path.join(plugin_dir,plugin_ini_fname)
  parser=ConfigParser.ConfigParser(defaults=dict(sources="",headers="", cflags="",cppflags="",cxxflags="", libs="", ldflags=""))
  parser.read(plugin_file)
  plugin=dict(parser.items('plugin'))
  plugin['rel_path']= plugin_dir[len(top_srcdir)+len(os.path.sep):]
  # TODO: determine path to plugin dir relative to top_srcdir... append it
  # to source files if they don't already have it
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
  
  if plugin.has_key('load_by_default'):
    plugin['load_by_default']=parser.getboolean('plugin','load_by_default')
  else:
    plugin['load_by_default']=False
  # Make a yes/no version for autoconf help messages
  if plugin['load_by_default']:
    plugin['default_yesno']="yes"
  else:
    plugin['default_yesno']="no"

  plugin_ac_file=os.path.join(plugin['rel_path'],'plugin.ac')
  plugin_am_file=os.path.join(plugin['rel_path'],'plugin.am')
  plugin_m4_dir=os.path.join(plugin['rel_path'],'m4')

  plugin_m4_files=[]
  if os.path.exists(plugin_m4_dir) and os.path.isdir(plugin_m4_dir):
    for m4_file in os.listdir(plugin_m4_dir):
      if os.path.splitext(m4_file)[-1] == '.m4':
        plugin_m4_files.append(os.path.join(plugin['rel_path'], m4_file))

  plugin['build_conditional_tag']= "BUILD_%s_PLUGIN" % plugin['name'].upper()
  if plugin.has_key('build_conditional'):
    plugin['has_build_conditional']=True
  else:
    plugin['has_build_conditional']=False
    plugin['build_conditional']='"x${with_%(name)s_plugin}" = "xyes"' %plugin

  # Turn this on later plugin_lib%(name)s_plugin_la_CPPFLAGS=$(AM_CPPFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN %(cppflags)s

  #
  # Write plugin build instructions into plugin.am file.
  #
  if plugin['headers'] != "":
    plugin_am.write("noinst_HEADERS += %(headers)s\n" % plugin)
  plugin_am.write("""
plugin_lib%(name)s_dir=${top_srcdir}/%(rel_path)s
EXTRA_DIST += %(rel_path)s/plugin.ini
if %(build_conditional_tag)s
  noinst_LTLIBRARIES+=plugin/lib%(name)s_plugin.la
  plugin_lib%(name)s_plugin_la_LIBADD=%(libs)s
  plugin_lib%(name)s_plugin_la_DEPENDENCIES=%(libs)s
  plugin_lib%(name)s_plugin_la_LDFLAGS=$(AM_LDFLAGS) %(ldflags)s
  plugin_lib%(name)s_plugin_la_CPPFLAGS=$(AM_CPPFLAGS) %(cppflags)s
  plugin_lib%(name)s_plugin_la_CXXFLAGS=$(AM_CXXFLAGS) %(cxxflags)s
  plugin_lib%(name)s_plugin_la_CFLAGS=$(AM_CFLAGS) %(cflags)s

  plugin_lib%(name)s_plugin_la_SOURCES=%(sources)s
endif
""" % plugin)
  # Add this once we're actually doing dlopen (and remove -avoid-version if
  # we move to ltdl
  #pkgplugin_LTLIBRARIES+=plugin/lib%(name)s_plugin.la
  #plugin_lib%(name)s_plugin_la_LDFLAGS=-module -avoid-version -rpath $(pkgplugindir) %(libs)s
  #drizzled_drizzled_LDADD+=${top_builddir}/plugin/lib%(name)s_plugin.la

  if os.path.exists(plugin_am_file):
    plugin_am.write('include %s\n' % plugin_am_file) 

  #
  # Write plugin config instructions into plugin.am file.
  #
  plugin_ac.write("\n\ndnl Config for %(title)s\n" % plugin)
  for m4_file in plugin_m4_files:
    plugin_ac.write('m4_sinclude([%s])\n' % m4_file) 

  plugin_ac.write("""
AC_ARG_WITH([plugin-%(name)s],[
dnl indented werid to make the help output correct
AS_HELP_STRING([--with-%(name)s-plugin],[Build %(title)s and enable it. @<:@default=%(default_yesno)s@:>@])
AS_HELP_STRING([--without-%(name)s-plugin],[Disable building %(title)s])
  ],
  [with_%(name)s_plugin="$withval"],
  [AS_IF([test "x$ac_with_all_plugins" = "yes"],
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
      [with_%(name)s_plugin=yes],
      [with_%(name)s_plugin=no])
  """ % plugin)
  plugin['plugin_dep_libs']=" ".join(["\${top_builddir}/%s" % f for f in plugin['libs'].split()])
  plugin_ac.write("""
AM_CONDITIONAL([%(build_conditional_tag)s],
               [test %(build_conditional)s])
AS_IF([test "x$with_%(name)s_plugin" = "xyes"],
      [
        drizzled_default_plugin_list="%(name)s,${drizzled_default_plugin_list}"
        drizzled_builtin_list="builtin_%(name)s_plugin,${drizzled_builtin_list}"
        drizzled_plugin_libs="${drizzled_plugin_libs} \${top_builddir}/plugin/lib%(name)s_plugin.la"
	DRIZZLED_PLUGIN_DEP_LIBS="${DRIZZLED_PLUGIN_DEP_LIBS} %(plugin_dep_libs)s"
      ])
""" % plugin)
 
plugin_ac.close()
plugin_am.close()

# We've written all of this out into .new files, now we only copy them
# over the old ones if they are different, so that we don't cause 
# unnecessary recompiles
for f in ('plugin.ac','plugin.am'):
  fname= os.path.join(top_builddir,'config',f)
  if os.system('diff %s.new %s >/dev/null 2>&1' % (fname,fname)):
    try:
      os.unlink(fname)
    except:
      pass
    os.rename('%s.new' % fname, fname)
  try:
    os.unlink("%s.new" % fname)
  except:
    pass
