/* Copyright (C) 2009 Sun Microsystems

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

#include <drizzled/server_includes.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin/listen_tcp.h>
#include <drizzled/plugin/client.h>

#include <iostream>

using namespace std;
using namespace drizzled;

static bool enable= false;
static bool debug= false;

class ClientConsole: public plugin::Client
{
  bool is_dead;
  uint32_t column;
  uint32_t max_column;

public:
  ClientConsole():
    is_dead(false),
    column(0),
    max_column(0)
  {}

  virtual void printDebug(const char *message)
  {
    if (debug)
      cout << "CONSOLE: " << message << endl;
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

  virtual bool isReading(void)
  {
    printDebug("isReading");
    return false;
  }

  virtual bool isWriting(void)
  {
    printDebug("isWriting");
    return false;
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
    return true;
  }

  virtual bool readCommand(char **packet, uint32_t *packet_length)
  {
    char buffer[8192];
    printf("drizzled> ");
    fflush(stdout);

    if (is_dead)
      return false;

    if (fgets(buffer, 8192, stdin) == NULL ||!strcasecmp(buffer, "quit\n") ||
        !strcasecmp(buffer, "exit\n"))
    {
      is_dead= true;
      *packet_length= 1;
      *packet= (char *)malloc(*packet_length);
      (*packet)[0]= COM_SHUTDOWN;
      return true;
    }

    *packet_length= strlen(buffer);
    *packet= (char *)malloc(*packet_length);
    (*packet)[0]= COM_QUERY;
    memcpy(*packet + 1, buffer, *packet_length - 1);

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

  virtual void sendError(uint32_t sql_errno, const char *err)
  {
    cout << "Error: " << sql_errno << " " << err << endl;
  }

  virtual bool sendFields(List<Item> *list)
  {
    List_iterator_fast<Item> it(*list);
    Item *item;

    column= 0;
    max_column= 0;

    while ((item=it++))
    {
      SendField field;
      item->make_field(&field);
      cout << field.col_name << "\t";
      max_column++;
    }

    cout << endl;

    return false;
  }

  virtual void checkRowEnd(void)
  {
    if (++column % max_column == 0)
      cout << endl;
  }

  using Client::store;

  virtual bool store(Field *from)
  {
    if (from->is_null())
      return store();

    char buff[MAX_FIELD_WIDTH];
    String str(buff, sizeof(buff), &my_charset_bin);
    from->val_str(&str);
    return store(str.ptr(), str.length());
  }

  virtual bool store(void)
  {
    cout << "NULL" << "\t";
    checkRowEnd();
    return false;
  }

  virtual bool store(int32_t from)
  {
    cout << from << "\t";
    checkRowEnd();
    return false;
  }

  virtual bool store(uint32_t from)
  {
    cout << from << "\t";
    checkRowEnd();
    return false;
  }

  virtual bool store(int64_t from)
  {
    cout << from << "\t";
    checkRowEnd();
    return false;
  }

  virtual bool store(uint64_t from)
  {
    cout << from << "\t";
    checkRowEnd();
    return false;
  }

  virtual bool store(double from, uint32_t decimals, String *buffer)
  {
    buffer->set_real(from, decimals, &my_charset_bin);
    return store(buffer->ptr(), buffer->length());
  }

  virtual bool store(const DRIZZLE_TIME *tm)
  {
    char buff[40];
    uint32_t length;
    uint32_t day;

    switch (tm->time_type)
    {
    case DRIZZLE_TIMESTAMP_DATETIME:
      length= sprintf(buff, "%04d-%02d-%02d %02d:%02d:%02d",
                      (int) tm->year,
                      (int) tm->month,
                      (int) tm->day,
                      (int) tm->hour,
                      (int) tm->minute,
                      (int) tm->second);
      if (tm->second_part)
        length+= sprintf(buff+length, ".%06d", (int)tm->second_part);
      break;

    case DRIZZLE_TIMESTAMP_DATE:
      length= sprintf(buff, "%04d-%02d-%02d",
                      (int) tm->year,
                      (int) tm->month,
                      (int) tm->day);
      break;

    case DRIZZLE_TIMESTAMP_TIME:
      day= (tm->year || tm->month) ? 0 : tm->day;
      length= sprintf(buff, "%s%02ld:%02d:%02d", tm->neg ? "-" : "",
                      (long) day*24L+(long) tm->hour, (int) tm->minute,
                      (int) tm->second);
      if (tm->second_part)
        length+= sprintf(buff+length, ".%06d", (int)tm->second_part);
      break;

    case DRIZZLE_TIMESTAMP_NONE:
    case DRIZZLE_TIMESTAMP_ERROR:
    default:
      assert(0);
      return false;
    }

    return store(buff);
  }

  virtual bool store(const char *from, size_t length)
  {
    printf("%.*s\t", (uint32_t)length, from);
    checkRowEnd();
    return false;
  }

  virtual bool haveMoreData(void)
  {
    printDebug("haveMoreData");
    return false;
  }

  virtual bool haveError(void)
  {
    printDebug("haveError");
    return false;
  }

  virtual bool wasAborted(void)
  {
    printDebug("wasAborted");
    return false;
  }
};

class ListenConsole: public plugin::Listen
{
  int pipe_fds[2];

public:
  ListenConsole()
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
    if (debug)
      enable= true;

    if (!enable)
      return false;

    if (pipe(pipe_fds) == -1)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("pipe() failed with errno %d"), errno);
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
    return new ClientConsole;
  }
};

static ListenConsole listen_obj;

static int init(drizzled::plugin::Registry &registry)
{
  registry.add(listen_obj);
  return 0;
}

static int deinit(drizzled::plugin::Registry &registry)
{
  registry.remove(listen_obj);
  return 0;
}

static DRIZZLE_SYSVAR_BOOL(enable, enable, PLUGIN_VAR_NOCMDARG,
                           N_("Enable the console."), NULL, NULL, false);

static DRIZZLE_SYSVAR_BOOL(debug, debug, PLUGIN_VAR_NOCMDARG,
                           N_("Turn on extra debugging."), NULL, NULL, false);

static struct st_mysql_sys_var* vars[]= {
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(debug),
  NULL
};

drizzle_declare_plugin(console)
{
  "console",
  "0.1",
  "Eric Day",
  "Console Client",
  PLUGIN_LICENSE_BSD,
  init,   /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  vars,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
