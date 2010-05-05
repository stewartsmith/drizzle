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

top_srcdir='.'
top_builddir='.'
plugin_ini_fname='plugin.ini'
plugin_list=[]
plugin_am_file=None
plugin_ac_file=None
plugin_prefix=''
plugin_suffix='_plugin'
extra_cflags=''
extra_cppflags=''
extra_cxxflags=' -DBUILDING_DRIZZLE'
root_plugin_dir='plugin'
pkgplugindir='$(libdir)/drizzle'
default_install='True'
default_plugin_version=''
force_lowercase_libname=True
