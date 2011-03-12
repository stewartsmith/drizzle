#  Copyright (C) 2011 Andrew Hutchings
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

def setup(app):
    try:
        app.add_description_unit('confval', 'confval',
                                objname='configuration value',
                                 indextemplate='pair: %s; configuration value')
    except TypeError:
        # We have to trap for this because of Sphinx 0.6.2
        pass
