/* Copyright (C) 2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>
#include <drizzled/field.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin/listen_tcp.h>
#include <drizzled/plugin/client.h>
#include <drizzled/session.h>
#include <drizzled/module/option_map.h>
#include <drizzled/plugin/catalog.h>
#include <drizzled/plugin.h>

#include <iostream>

#include <boost/program_options.hpp>

#include <client/user_detect.h>

using namespace std;
using namespace drizzled;

namespace po= boost::program_options;

static bool enabled= false;
static bool debug_enabled= false;


class ClientConsole: public plugin::Client
{
  bool is_dead;
  uint32_t column;
  uint32_t max_column;
  const std::string &username;
  const std::string &password;
  const std::string &schema;
  const std::string &_catalog;

public:
  ClientConsole(const std::string &username_arg,
                const std::string &password_arg,
                const std::string &schema_arg,
                const std::string &catalog_arg) :
    is_dead(false),
    column(0),
    max_column(0),
    username(username_arg),
    password(password_arg),
    schema(schema_arg),
    _catalog(catalog_arg)
  {}

  virtual void printDebug(const char *message)
  {
    if (debug_enabled)
      cout << "CONSOLE: " << message << endl;
  }

  catalog::Instance::shared_ptr catalog()
  {
    identifier::Catalog identifier(_catalog);
    catalog::Instance::shared_ptr tmp= plugin::Catalog::getInstance(identifier);
    if (not tmp)
    {
      std::cerr << "Invalid catalog '" << identifier << "', resorting to 'local' catalog" << std::endl;
    }
    return tmp;
  }

  virtual int getFileDescriptor(void)
  {
    printDebug("getFileDescriptor");
    return 0;
  }

  virtual bool isConnected(void)
  {
    printDebug("isConnected");
    return true;
  }

  virtual bool flush(void)
  {
    printDebug("flush");
    return false;
  }

  virtual void close(void)
  {
    printDebug("close");
    is_dead= true;
  }

  virtual bool authenticate(void)
  {
    printDebug("authenticate");
    identifier::user::mptr user= identifier::User::make_shared();
    user->setUser(username);
    session->setUser(user);

    return session->checkUser(password, schema);
  }

  virtual bool readCommand(char **packet, uint32_t& packet_length)
  {
    uint32_t length;

    if (is_dead)
      return false;

    cout << "drizzled> ";

    length= 1024;
    *packet= NULL;

    /* Start with 1 byte offset so we can set command. */
    packet_length= 1;

    do
    {
      *packet= (char *)realloc(*packet, length);
      if (*packet == NULL)
        return false;

      cin.clear();
      cin.getline(*packet + packet_length, length - packet_length, ';');
      packet_length+= cin.gcount();
      length*= 2;
    }
    while (cin.eof() == false && cin.fail() == true);

    if ((packet_length == 1 && cin.eof() == true) or
        not strncasecmp(*packet + 1, "quit", 4) or
        not strncasecmp(*packet + 1, "exit", 4) or
        not strncasecmp(*packet + 1, "shutdown", sizeof("shutdown") -1))
    {
      is_dead= true;
      packet_length= 1;
      (*packet)[0]= COM_SHUTDOWN;

      return true;
    }

    /* Skip \r and \n for next time. */
    cin.ignore(2, '\n');

    (*packet)[0]= COM_QUERY;

    return true;
  }

  virtual void sendOK(void)
  {
    cout << "OK" << endl;
  }

  virtual void sendEOF(void)
  {
    printDebug("sendEOF");
  }

  virtual void sendError(const drizzled::error_t sql_errno, const char *err)
  {
    cout << "Error: " << static_cast<long>(sql_errno) << " " << err << endl;
  }

  virtual void sendFields(List<Item>& list)
  {
    List<Item>::iterator it(list.begin());

    column= 0;
    max_column= 0;

    while (Item* item=it++)
    {
      SendField field;
      item->make_field(&field);
      cout << field.col_name << "\t";
      max_column++;
    }
    cout << endl;
  }

  virtual void checkRowEnd(void)
  {
    if (++column % max_column == 0)
      cout << endl;
  }

  using Client::store;

  virtual void store(Field *from)
  {
    if (from->is_null())
      return store();

    char buff[MAX_FIELD_WIDTH];
    String str(buff, sizeof(buff), &my_charset_bin);
    from->val_str_internal(&str);
    return store(str.ptr(), str.length());
  }

  virtual void store(void)
  {
    cout << "NULL" << "\t";
    checkRowEnd();
  }

  virtual void store(int32_t from)
  {
    cout << from << "\t";
    checkRowEnd();
  }

  virtual void store(uint32_t from)
  {
    cout << from << "\t";
    checkRowEnd();
  }

  virtual void store(int64_t from)
  {
    cout << from << "\t";
    checkRowEnd();
  }

  virtual void store(uint64_t from)
  {
    cout << from << "\t";
    checkRowEnd();
  }

  virtual void store(double from, uint32_t decimals, String *buffer)
  {
    buffer->set_real(from, decimals, &my_charset_bin);
    store(buffer->ptr(), buffer->length());
  }

  virtual void store(const char *from, size_t length)
  {
    cout.write(from, length);
    cout << "\t";
    checkRowEnd();
  }

  virtual bool haveError()
  {
    printDebug("haveError");
    return false;
  }

  virtual bool wasAborted()
  {
    printDebug("wasAborted");
    return false;
  }

  bool isConsole() const
  {
    return true;
  }

  bool isInteractive() const
  {
    return true;
  }
};

class ListenConsole: public plugin::Listen
{
  int pipe_fds[2];
  const std::string username;
  const std::string password;
  const std::string schema;
  const std::string _catalog;

public:
  ListenConsole(const std::string &name_arg,
                const std::string &username_arg,
                const std::string &password_arg,
                const std::string &schema_arg,
                const std::string &catalog_arg) :
    plugin::Listen(name_arg),
    username(username_arg),
    password(password_arg),
    schema(schema_arg),
    _catalog(catalog_arg)
  {
    pipe_fds[0]= -1;
  }

  virtual ~ListenConsole()
  {
    if (pipe_fds[0] != -1)
    {
      close(pipe_fds[0]);
      close(pipe_fds[1]);
    }
  }

  virtual bool getFileDescriptors(std::vector<int> &fds)
  {
    if (debug_enabled)
      enabled= true;

    if (not enabled)
      return false;

    if (pipe(pipe_fds) == -1)
    {
      errmsg_printf(error::ERROR, _("pipe() failed with errno %d"), errno);
      return true;
    }

    fds.push_back(pipe_fds[0]);
    assert(write(pipe_fds[1], "\0", 1) == 1);
    return false;
  }

  virtual drizzled::plugin::Client *getClient(int fd)
  {
    char buffer[1];
    assert(read(fd, buffer, 1) == 1);

    return new ClientConsole(username, password, schema, _catalog);
  }
};

static int init(drizzled::module::Context &context)
{
  const module::option_map &vm= context.getOptions();
  context.add(new ListenConsole("console", vm["username"].as<string>(), 
    vm["password"].as<string>(), vm["schema"].as<string>(), vm["catalog"].as<string>()));
  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("enable",
          po::value<bool>(&enabled)->default_value(false)->zero_tokens(),
          N_("Enable the console."));
  context("debug",
          po::value<bool>(&debug_enabled)->default_value(false)->zero_tokens(),
          N_("Turn on extra debugging."));
  UserDetect detected_user;
  const char* shell_user= detected_user.getUser();
  context("username",
          po::value<string>()->default_value(shell_user ? shell_user : ""),
          N_("User to use for auth."));
  context("password",
          po::value<string>()->default_value(""),
          N_("Password to use for auth."));
  context("catalog",
          po::value<string>()->default_value("LOCAL"),
          N_("Default catalog to use."));
  context("schema",
          po::value<string>()->default_value(""),
          N_("Default schema to use."));
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "console",
  "0.2",
  "Eric Day",
  "Console Client",
  PLUGIN_LICENSE_BSD,
  init,   /* Plugin Init */
  NULL,   /* depends */
  init_options    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
