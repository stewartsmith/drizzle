/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
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
 * @file
 *
 * Defines the implementation of the default serial event log.
 *
 * @see drizzled/plugin/replicator.h
 * @see drizzled/plugin/applier.h
 *
 * @details
 *
 * Currently, this is a very simple implementation.  We have a global
 * lock on the file writer and write the events as the are received, 
 * first writing a 64-bit length and then the serialized transaction/command.
 *
 * @todo
 *
 * Possibly look at a scoreboard approach with multiple file segments.  For
 * right now, though, this is just a quick simple implementation to serve
 * as a skeleton and a springboard.
 */

#include "serial_event_log.h"

#include <drizzled/gettext.h>
#include <drizzled/message/transaction.pb.h>

#include <vector>
#include <string>

using namespace std;

/** 
 * Serial Event Log plugin system variable - Is the log enabled? Only used on init().  
 * The enable() and disable() methods of the SerialEventLog class control online
 * disabling.
 */
static bool sysvar_serial_event_log_enabled= false;
/** Serial Event Log plugin system variable - The path to the log file used */
static char* sysvar_serial_event_log_file= NULL;
static const char DEFAULT_LOG_FILE_PATH[16]= "event.log"; /* In datadir... */

SerialEventLog::SerialEventLog(const char *in_log_file_path)
  : 
    drizzled::plugin::Applier(),
    state(SerialEventLog::OFFLINE),
    log_file_path(in_log_file_path)
{
  is_enabled= true; /* If constructed, the plugin is enabled until taken offline with disable() */
  is_active= false;

  /* Setup our log file and determine the next write offset... */
  log_file= open(log_file_path, O_APPEND|O_CREAT|O_SYNC|O_WRONLY, S_IRWXU);
  if (log_file == -1)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to open serial event log file.  Got error: %s"), strerror(errno));
    is_active= false;
    return;
  }

  /* 
   * The offset of the next write is the current position of the log
   * file, since it's opened in append mode...
   */
  log_offset= lseek(log_file, 0, SEEK_END);

  state= SerialEventLog::ONLINE;
  is_active= true;
}

SerialEventLog::~SerialEventLog()
{
  /* Clear up any resources we've consumed */
  if (is_active && log_file != -1)
  {
    (void) close(log_file);
  }
}

bool SerialEventLog::isActive()
{
  return is_enabled && is_active;
}

void SerialEventLog::apply(drizzled::message::Command *to_apply)
{
  std::string buffer; /* Buffer we will write serialized command to */
  size_t length;
  size_t written;
  off_t cur_offset;

  to_apply->SerializeToString(&buffer);
  length= buffer.length();

  /*
   * Do an atomic increment on the offset of the log file position
   */
  cur_offset= log_offset+= (sizeof(uint64_t) + length);
  /** 
   * @TODO
   *
   * Not sure about the following problem:
   *
   * If log_offset is incremented by thread 2 *before* cur_offset
   * is assigned to the log_offset value, then thread 2's write will
   * clobber thread 1's write since the cur_offset will be wrong.
   *
   * Do we need to do the following check?
   *
   * if (unlikely(cur_offset != log_offset))
   * {
   *   usleep(random_time_period);
   *   restart from beginning...
   * }
   */
  /*
   * We adjust cur_offset back to the original log_offset before
   * the increment above...
   */
  cur_offset-= (sizeof(uint64_t) + length);

  /* 
   * Quick safety...if an error occurs below, the log file will
   * not be active, therefore a caller could have been ready
   * to write...but the log is crashed.
   */
  if (unlikely(state == SerialEventLog::CRASHED))
    return;

  written= pwrite(log_file, &length, sizeof(uint64_t), cur_offset);
  cur_offset+= written;
  if (unlikely(written != sizeof(uint64_t)))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, 
                  _("Failed to write full size of command.  Tried to write %" PRId64 ", but only wrote %" PRId64 ".  Error: %s"), 
                  (uint64_t) length, 
                  (uint64_t) written, 
                  strerror(errno));
    state= CRASHED;
    is_active= false;
    return;
  }

  /* 
   * Quick safety...if an error occurs above in another writer, the log 
   * file will be in a crashed state.
   */
  if (unlikely(state == SerialEventLog::CRASHED))
    return;

  written= pwrite(log_file, buffer.c_str(), length, cur_offset);
  if (unlikely(written != length))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, 
                  _("Failed to write full serialized command.  Tried to write %" PRId64 ", but only wrote %" PRId64 ".  Error: %s"), 
                  (uint64_t) length, 
                  (uint64_t) written, 
                  strerror(errno));
    state= CRASHED;
    is_active= false;
    return;
  }
}

static SerialEventLog *serial_event_log= NULL; /* The singleton serial log */

static int init(PluginRegistry &registry)
{
  if (sysvar_serial_event_log_enabled)
    serial_event_log= new SerialEventLog(sysvar_serial_event_log_file);
  registry.add(serial_event_log);
  return 0;
}

static int deinit(PluginRegistry &registry)
{
  if (serial_event_log)
  {
    registry.remove(serial_event_log);
    delete serial_event_log;
  }
  return 0;
}

static DRIZZLE_SYSVAR_BOOL(enable,
                          sysvar_serial_event_log_enabled,
                          PLUGIN_VAR_NOCMDARG,
                          N_("Enable serial event log"),
                          NULL, /* check func */
                          NULL, /* update func */
                          false /* default */);

static DRIZZLE_SYSVAR_STR(log_file,
                          sysvar_serial_event_log_file,
                          PLUGIN_VAR_READONLY,
                          N_("Path to the file to use for serial event log."),
                          NULL, /* check func */
                          NULL, /* update func*/
                          DEFAULT_LOG_FILE_PATH /* default */);

static struct st_mysql_sys_var* system_variables[]= {
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(log_file),
  NULL
};

drizzle_declare_plugin(serial_event_log)
{
  "serial_event_log",
  "0.1",
  "Jay Pipes",
  N_("Default Serial Event Log"),
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL, /* status variables */
  system_variables, /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
