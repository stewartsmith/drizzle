#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2011 Patrick Crews
#
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import unittest
import subprocess

from lib.util.sqlbench_methods import execute_sqlbench
from lib.util.mysqlBaseTestCase import mysqlBaseTestCase
from lib.util.mailing_report import kewpieSendMail
from lib.opts.test_run_options import parse_qp_options

server_requirements = [[]]
servers = []
server_manager = None
test_executor = None

class basicTest(mysqlBaseTestCase):

    def test_run_all_sqlbench(self):
        test_cmd = "$SQLBENCH_DIR/run-all-tests --server=drizzle --dir=$DRIZZLE_TEST_WORKDIR --log --connect-options=port=$MASTER_MYPORT --create-options=ENGINE=innodb --user=root"

        test_status, retcode, output = execute_sqlbench(test_cmd, test_executor, servers)
        self.assertEqual(retcode, 0, msg = output)
        self.assertEqual(test_status, 'pass', msg = output)

        # sending test report via mail
        if mail_tgt:
            kewpieSendMail(test_executor,mail_tgt,test_status)

    def tearDown(self):
            server_manager.reset_servers(test_executor.name)

