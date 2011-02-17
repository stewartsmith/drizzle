/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor
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

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <boost/program_options.hpp>

#include <drizzled/module/option_context.h>

namespace po=boost::program_options;

using namespace drizzled;

BOOST_AUTO_TEST_SUITE(OptionContext)
BOOST_AUTO_TEST_CASE(parsing)
{
  const std::string module_name("test");
  po::options_description command_line_options("Test prefix injection");
  module::option_context ctx(module_name, command_line_options.add_options());

  ctx("option", po::value<std::string>(), "Test option name prefix injection");

  po::variables_map vm;

  const char *options[]= {
    "test", "--test.option=foo"
  };

  // Disable allow_guessing
  int style = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;

  po::store(po::command_line_parser(2, (char **)options).style(style).
            options(command_line_options).run(), vm);
  po::notify(vm);

  BOOST_REQUIRE_EQUAL(0, vm.count("option"));
  BOOST_REQUIRE_EQUAL(1, vm.count("test.option"));
  BOOST_REQUIRE_EQUAL(0, vm["test.option"].as<std::string>().compare("foo"));
}
BOOST_AUTO_TEST_SUITE_END()
