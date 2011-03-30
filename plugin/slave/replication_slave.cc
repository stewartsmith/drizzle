/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 David Shrewsbury
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
#include <plugin/slave/replication_slave.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/program_options/config_file.h>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <drizzled/plugin.h>

using namespace std;
using namespace drizzled;

namespace po= boost::program_options;

namespace slave
{

void ReplicationSlave::startup(Session &session)
{
  (void)session;
  if (not initWithConfig())
  {
    errmsg_printf(error::ERROR, _("Could not start slave services: %s\n"),
                  _error.c_str());
  }
  else
  {
    /* Start the IO threads */
    boost::unordered_map<uint32_t, Master *>::const_iterator it;
    for (it= _masters.begin(); it != _masters.end(); ++it)
    {
      it->second->start();
      /* Consumer must know server IDs */
      _consumer.addMasterId(it->first);
    }

    _consumer_thread= boost::thread(&QueueConsumer::run, &_consumer);
  }
}

bool ReplicationSlave::initWithConfig()
{
  po::variables_map vm;
  po::options_description slave_options("Options for the slave plugin");

  /* Common options */
  slave_options.add_options()
    ("seconds-between-reconnects", po::value<uint32_t>()->default_value(30))
    ("io-thread-sleep", po::value<uint32_t>()->default_value(5))
    ("applier-thread-sleep", po::value<uint32_t>()->default_value(5))
    ("ignore-errors", po::value<bool>()->default_value(false)->zero_tokens());

  /* Master defining options */
  for (size_t num= 1; num <= 10; num++)
  {
    string section("master");
    section.append(boost::lexical_cast<string>(num));
    slave_options.add_options()
      ((section + ".master-host").c_str(), po::value<string>()->default_value(""))
      ((section + ".master-port").c_str(), po::value<uint16_t>()->default_value(3306))
      ((section + ".master-user").c_str(), po::value<string>()->default_value(""))
      ((section + ".master-pass").c_str(), po::value<string>()->default_value(""))
      ((section + ".max-reconnects").c_str(), po::value<uint32_t>()->default_value(10))
      ((section + ".max-commit-id").c_str(), po::value<uint64_t>());
   }

  ifstream cf_stream(_config_file.c_str());

  if (not cf_stream.is_open())
  {
    _error= "Unable to open file ";
    _error.append(_config_file);
    return false;
  }

  po::store(drizzled::program_options::parse_config_file(cf_stream, slave_options), vm);

  po::notify(vm);

  /*
   * We will support 10 masters. This loope effectively creates the Master
   * objects as they are referenced.
   *
   * @todo Support a variable number of master hosts.
   */
  for (size_t num= 1; num <= 10; num++)
  {
    string section("master");
    section.append(boost::lexical_cast<string>(num));

    /* WARNING! Hack!
     * We need to be able to determine when a master host is actually defined
     * by the user vs. we are just using defaults. So if the hostname is ever
     * the default value of "", then we'll assume that this section was not
     * user defined.
     */
    if (vm[section + ".master-host"].as<string>() == "")
      continue;

    _masters[num]= new (std::nothrow) Master(num);

    if (vm.count(section + ".master-host"))
      master(num).producer().setMasterHost(vm[section + ".master-host"].as<string>());

    if (vm.count(section + ".master-port"))
      master(num).producer().setMasterPort(vm[section + ".master-port"].as<uint16_t>());

    if (vm.count(section + ".master-user"))
      master(num).producer().setMasterUser(vm[section + ".master-user"].as<string>());

    if (vm.count(section + ".master-pass"))
      master(num).producer().setMasterPassword(vm[section + ".master-pass"].as<string>());

    if (vm.count(section + ".max-commit-id"))
      master(num).producer().setCachedMaxCommitId(vm[section + ".max-commit-id"].as<uint64_t>());
  }

  boost::unordered_map<uint32_t, Master *>::const_iterator it;

  for (it= _masters.begin(); it != _masters.end(); ++it)
  {
    if (vm.count("max-reconnects"))
      it->second->producer().setMaxReconnectAttempts(vm["max-reconnects"].as<uint32_t>());

    if (vm.count("seconds-between-reconnects"))
      it->second->producer().setSecondsBetweenReconnects(vm["seconds-between-reconnects"].as<uint32_t>());

    if (vm.count("io-thread-sleep"))
      it->second->producer().setSleepInterval(vm["io-thread-sleep"].as<uint32_t>());
  }

  if (vm.count("applier-thread-sleep"))
    _consumer.setSleepInterval(vm["applier-thread-sleep"].as<uint32_t>());
  if (vm.count("ignore-errors"))
    _consumer.setIgnoreErrors(vm["ignore-errors"].as<bool>());

  /* setup schema and tables */
  ReplicationSchema rs;
  if (not rs.create())
  {
    _error= rs.getErrorMessage();
    return false;
  }

  for (it= _masters.begin(); it != _masters.end(); ++it)
  {
    /* make certain a row exists for each master */
    rs.createInitialApplierRow(it->first);
    rs.createInitialIORow(it->first);

    uint64_t cachedValue= it->second->producer().cachedMaxCommitId();
    if (cachedValue)
    {
      if (not rs.setInitialMaxCommitId(it->first, cachedValue))
      {
        _error= rs.getErrorMessage();
        return false;
      }
    }
  }

  return true;
}

} /* namespace slave */
