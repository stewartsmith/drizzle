#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2012 M.Sharan Kumar
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

import smtplib

def sendMail(mail_executor,to_address,message):
    """mails the report of sysbench test"""

    logging=mail_executor.logging

    #configuring sender's login
    from_address='smtplibpython@gmail.com'
    from_password='smtpprotocol'

    #sending mail to specified to_addresses
    logging.info("Mailing report...")
    logging.info("To: %s" %to_address)
    logging.info("From: %s" %from_address)
    server=smtplib.SMTP('smtp.gmail.com',587)
    server.ehlo()
    server.starttls()
    server.ehlo()
    server.login(from_address,from_password)
    server.sendmail(from_address,to_address,message)
    logging.info("Report successfully sent...")
    server.close()
