# Copyright (C) 2000-2006 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

## Process this file with automake to create Makefile.in


bin_SCRIPTS =		mysqld_safe \
			mysql_install_db \
			mysql_config \
			mysql_fix_extensions \
			mysql_convert_table_format \
			mysql_find_rows \
			mysqlhotcopy \
			mysqldumpslow \
			mysqld_multi

noinst_SCRIPTS =	make_binary_distribution \
			make_sharedlib_distribution

EXTRA_SCRIPTS =		make_binary_distribution.sh \
			make_sharedlib_distribution.sh \
			mysql_config.sh \
			mysql_config.pl.in \
			mysql_fix_extensions.sh \
			mysql_install_db.sh \
			mysql_convert_table_format.sh \
			mysql_find_rows.sh \
			mysqlhotcopy.sh \
			mysqldumpslow.sh \
			mysqld_multi.sh \
			mysqld_safe.sh

EXTRA_DIST =		$(EXTRA_SCRIPTS) \
			mysql_system_tables_fix.sql

dist_pkgdata_DATA =	fill_help_tables.sql \
			mysql_system_tables.sql \
			mysql_test_data_timezone.sql

CLEANFILES =		mysqld_safe \
			mysql_install_db \
			make_binary_distribution \
			make_sharedlib_distribution \
			mysql_config \
			mysql_fix_extensions \
			mysql_convert_table_format \
			mysql_find_rows \
			mysqlhotcopy \
			mysqldumpslow \
			mysqld_multi

pkgplugindir =		$(pkglibdir)/plugin

# Default same as 'pkgdatadir', but we can override it
pkgsuppdir =		$(datadir)/@PACKAGE@


SUFFIXES = .sh

.sh:
	@RM@ -f $@ $@-t
	@SED@ \
	  -e 's!@''bindir''@!$(bindir)!g' \
	  -e 's!@''sbindir''@!$(sbindir)!g' \
	  -e 's!@''scriptdir''@!$(bindir)!g' \
	  -e 's!@''prefix''@!$(prefix)!g' \
	  -e 's!@''datadir''@!$(datadir)!g' \
	  -e 's!@''localstatedir''@!$(localstatedir)!g' \
	  -e 's!@''libexecdir''@!$(libexecdir)!g' \
	  -e 's!@''pkglibdir''@!$(pkglibdir)!g' \
	  -e 's!@''pkgincludedir''@!$(pkgincludedir)!g' \
	  -e 's!@''pkgdatadir''@!$(pkgdatadir)!g' \
	  -e 's!@''pkgplugindir''@!$(pkgplugindir)!g' \
	  -e 's!@''pkgsuppdir''@!$(pkgsuppdir)!g' \
	  -e 's!@''sysconfdir''@!$(sysconfdir)!g' \
	  -e 's!@''mandir''@!$(mandir)!g' \
	  -e 's!@''infodir''@!$(infodir)!g' \
	  -e 's!@''CC''@!@CC@!'\
	  -e 's!@''CXX''@!@CXX@!'\
	  -e 's!@''GXX''@!@GXX@!'\
	  -e 's!@''SAVE_CC''@!@SAVE_CC@!'\
	  -e 's!@''SAVE_CXX''@!@SAVE_CXX@!'\
	  -e 's!@''CC_VERSION''@!@CC_VERSION@!'\
	  -e 's!@''CXX_VERSION''@!@CXX_VERSION@!'\
	  -e 's!@''PERL''@!@PERL@!' \
	  -e 's!@''SAVE_ASFLAGS''@!@SAVE_ASFLAGS@!'\
	  -e 's!@''SAVE_CFLAGS''@!@SAVE_CFLAGS@!'\
	  -e 's!@''SAVE_CXXFLAGS''@!@SAVE_CXXFLAGS@!'\
	  -e 's!@''SAVE_LDFLAGS''@!@SAVE_LDFLAGS@!'\
	  -e 's!@''ASFLAGS''@!@ASFLAGS@!'\
	  -e 's!@''CFLAGS''@!@CFLAGS@!'\
	  -e 's!@''CXXFLAGS''@!@CXXFLAGS@!'\
	  -e 's!@''LDFLAGS''@!@LDFLAGS@!'\
	  -e 's!@''CLIENT_LIBS''@!@CLIENT_LIBS@!' \
	  -e 's!@''LIBS''@!@LIBS@!' \
	  -e 's!@''WRAPLIBS''@!@WRAPLIBS@!' \
	  -e 's!@''innodb_system_libs''@!@innodb_system_libs@!' \
	  -e 's!@''openssl_libs''@!@openssl_libs@!' \
	  -e 's!@''VERSION''@!@VERSION@!' \
	  -e 's!@''MYSQL_BASE_VERSION''@!@MYSQL_BASE_VERSION@!' \
	  -e 's!@''MYSQL_SERVER_SUFFIX''@!@MYSQL_SERVER_SUFFIX@!' \
	  -e 's!@''COMPILATION_COMMENT''@!@COMPILATION_COMMENT@!' \
	  -e 's!@''MACHINE_TYPE''@!@MACHINE_TYPE@!' \
	  -e 's!@''HOSTNAME''@!@HOSTNAME@!' \
	  -e 's!@''SYSTEM_TYPE''@!@SYSTEM_TYPE@!' \
	  -e 's!@''CHECK_PID''@!@CHECK_PID@!' \
	  -e 's!@''FIND_PROC''@!@FIND_PROC@!' \
	  -e 's!@''MYSQLD_DEFAULT_SWITCHES''@!@MYSQLD_DEFAULT_SWITCHES@!' \
	  -e 's!@''MYSQL_UNIX_ADDR''@!@MYSQL_UNIX_ADDR@!' \
	  -e 's!@''MYSQL_TCP_PORT''@!@MYSQL_TCP_PORT@!' \
	  -e 's!@''MYSQL_TCP_PORT_DEFAULT''@!@MYSQL_TCP_PORT_DEFAULT@!' \
	  -e 's!@''TARGET_LINUX''@!@TARGET_LINUX@!' \
	  -e "s!@""CONF_COMMAND""@!@CONF_COMMAND@!" \
	  -e 's!@''MYSQLD_USER''@!@MYSQLD_USER@!' \
	  -e "s!@MAKE@!$(MAKE)!" \
	$< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@

