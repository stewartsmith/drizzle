/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2012 Mohit Srivastava
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
/**
 * @file Implements a class HTTPServer handles socket related functions. Based on example code https://gist.github.com/665437 .
 */
#include <event.h>
#include <evhttp.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
using namespace std;
/**
 *  Drizzle Plugin Namespace
 */
namespace drizzle_plugin {
/**
 *  Json Server Plugin Namespace
 */
namespace json_server{

class HTTPServer 
{
  public:
    /**
     * Constructor
     */
    HTTPServer() {}
    /**
     * Destructor
     */
    ~HTTPServer() {}
  protected:
    /**
     * Bind socket to a address and port.
     *
     * @param address a constant character pointer.
     * @param port a integer.
     *
     * @return a non-negative file descriptor on success or -1 on failure.
     */
    int BindSocket(const char *address, int port);
};

}
}
