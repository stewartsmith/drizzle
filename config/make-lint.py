#!/usr/bin/env python
#  Copyright (C) 2009 Sun Microsystems
#  Copyright (C) 2009 Robert Collins
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

import os.path
srcdir = os.environ.get('srcdir','.')
path = os.path.join(srcdir, 'config', 'lint-rules.am')

# relpath import (available in Python 2.6 and above)
try:
    relpath = os.path.relpath
except AttributeError:

    from os.path import curdir, sep, pardir, join

    def relpath(path, start=curdir):
        """Return a relative version of a path"""

        if not path:
            raise ValueError("no path specified")

        start_list = os.path.abspath(start).split(sep)
        path_list = os.path.abspath(path).split(sep)

        # Work out how much of the filepath is shared by start and path.
        i = len(os.path.commonprefix([start_list, path_list]))

        rel_list = [pardir] * (len(start_list)-i) + path_list[i:]
        if not rel_list:
            return curdir
        return join(*rel_list)

class ChangeProtectedFile(object):
  # from register_plugins.py. XXX: duplication. fix.

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
output = ChangeProtectedFile(path)

# We write a makefile that causes:
# linted to depend on linting all the source files we find
# linting a source file to depend on the output dep file for that linted file.


def lint_path(path):
    # linted depends on linting this:
    output.write('linted: %s.linted\n' % path)
    output.write('%s.linted: %s\n' % (path, path))
    # the thing being linted depends on the dependencies included in
    # the lint output
    #output.write('@am__include@ @am__quote@%s.linted@am__quote@\n' % path)
    # If the lint file doesn't exist, we'll make one, or else we have to do
    # a full lint run on every fresh bzr checkout, which is sort of silly
    #if not os.path.exists("%s.linted" % path):
    #    lint_file = open("%s.linted" % path,"w")
    #    lint_file.write("# Placeholder to make empty file")
    #    lint_file.close()


def clean_lints(paths):
    output.write('cleanlints:\n')
    # batch in 50
    for pos in range(len(paths)/50 + 1):
        path_str = ' '.join((path + '.linted') for path in paths[pos *50:(pos + 1)*50])
        if not path_str:
            continue
        output.write('\trm -f %s\n' % path_str)


def should_lint(path):
    if not (path.endswith('.cc') or path.endswith('.h')):
        return False
    if not (path.startswith('plugin/') or path.startswith('drizzled/') or
        path.startswith('client/')):
        return False
    # Let's not lint emacs autosave files
    if (os.path.split(path)[-1].startswith('.#')):
        return False
    # We need to figure out how to better express this
    for exclude in ['innobase', 'pbxt', 'pbms', 'gnulib', '.pb.', 'bak-header', 'm4',
        'sql_yacc', 'gperf', 'drizzled/probes.h',
        'drizzled/function_hash.h', 'drizzled/symbol_hash.h',
        'util/dummy.cc', 'drizzled/sql_yacc.h', 'drizzled/configmake.h',
	'drizzled/plugin/version.h',
        'drizzled/generated_probes.h',
        'drizzled/module/load_list.h']:
        if exclude in path:
            return False
    return True

def accumulate_sources(arg, dirname, fnames):
    for fname in fnames:
        path = os.path.join(dirname, fname)
        path = relpath(path, srcdir)
        if not should_lint(path):
            continue
        arg.append(path)

sources_list = []
os.path.walk(srcdir,accumulate_sources,sources_list)
sources_list.sort()
for path in sources_list:
    lint_path(path)
clean_lints(sources_list)

output.close()
