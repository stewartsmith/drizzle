/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Joseph Daly <skinny.moey@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <client/client_priv.h>
#include <string>


class TransactionLogConnection
{
public:
  TransactionLogConnection(std::string &host, uint16_t port,
                           std::string &username, std::string &password,
                           bool drizzle_protocol);

  ~TransactionLogConnection();


  void query(const std::string &str_query, drizzle_result_st *result);

  void errorHandler(drizzle_result_st *res,  drizzle_return_t ret, const char *when);

private:
  drizzle_st drizzle;
  drizzle_con_st connection;
  std::string hostName;
  bool drizzleProtocol;
};

