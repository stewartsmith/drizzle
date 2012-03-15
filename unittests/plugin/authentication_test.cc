/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
*
*  Copyright (C) 2010 Pawel Blokus
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

#include <config.h>

#include <drizzled/error.h>
#include <drizzled/plugin/authentication.h>
#include <drizzled/identifier.h>
#include <gtest/gtest.h>
#include <string>

#include "unittests/plugin/plugin_stubs.h"

using namespace drizzled;

static void error_handler_func_stub(drizzled::error_t my_err, const char *str, myf MyFlags)
{
  (void)my_err;
  (void)str;
  (void)MyFlags;
}

class AuthenticationTest : public ::testing::Test
{
public:
  identifier::user::ptr sctx;
  std::string passwd;
  AuthenticationStub stub1;
  AuthenticationStub stub2;

  AuthenticationTest() :
    sctx(drizzled::identifier::User::make_shared()),
    stub1("AuthenticationStub1"),
    stub2("AuthenticationStub2")
  {
  }
  
  virtual void SetUp ()
  {
    error_handler_hook = error_handler_func_stub;
  }

  void addOnePlugin()
  {
    plugin::Authentication::addPlugin(&stub1);
  }

  void removeOnePlugin()
  {
    plugin::Authentication::removePlugin(&stub1);
  }

  void addTwoPlugins()
  {
    plugin::Authentication::addPlugin(&stub1);
    plugin::Authentication::addPlugin(&stub2);
  }

  void removeTwoPlugins()
  {
    plugin::Authentication::removePlugin(&stub1);
    plugin::Authentication::removePlugin(&stub2);
  }
  
};

TEST_F(AuthenticationTest, isAuthenticated_noPluginsLoaded_shouldReturn_True)
{
  bool authenticated = plugin::Authentication::isAuthenticated(sctx, passwd);

  ASSERT_TRUE(authenticated);
}

TEST_F(AuthenticationTest, isAuthenticated_OnePluginReturnigFalse_ShouldReturn_False)
{

  addOnePlugin();
  bool authenticated = plugin::Authentication::isAuthenticated(sctx, passwd);
  removeOnePlugin();

  ASSERT_FALSE(authenticated);
}

TEST_F(AuthenticationTest, isAuthenticated_OnePluginReturnigTrue_ShouldReturn_True)
{

  stub1.set_authenticate_return(true);
  
  addOnePlugin();
  bool authenticated = plugin::Authentication::isAuthenticated(sctx, passwd);
  removeOnePlugin();
  
  ASSERT_TRUE(authenticated);
}

TEST_F(AuthenticationTest, isAuthenticated_TwoPluginsBothReturnigFalse_ShouldReturn_False)
{

  addTwoPlugins();
  bool authenticated = plugin::Authentication::isAuthenticated(sctx, passwd);
  removeTwoPlugins();
  
  ASSERT_FALSE(authenticated);
}

TEST_F(AuthenticationTest, isAuthenticated_TwoPluginsOnlyOneReturnigTrue_ShouldReturn_True)
{
  stub2.set_authenticate_return(true);
  
  addTwoPlugins();
  bool authenticated = plugin::Authentication::isAuthenticated(sctx, passwd);
  removeTwoPlugins();
  
  ASSERT_TRUE(authenticated);
}


