/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef PLUGIN_INFORMATION_ENGINE_INFORMATION_ENGINE_H
#define PLUGIN_INFORMATION_ENGINE_INFORMATION_ENGINE_H

#include <drizzled/thr_lock.h>
#include <drizzled/plugin/info_schema_table.h>
#include <drizzled/plugin/storage_engine.h>

static const char *information_exts[] = {
  NULL
};


class InformationEngine : public drizzled::plugin::StorageEngine
{
public:
  InformationEngine(const std::string &name_arg)
    : drizzled::plugin::StorageEngine(name_arg,
                                      drizzled::HTON_ALTER_NOT_SUPPORTED |
                                      drizzled::HTON_SKIP_STORE_LOCK |
                                      drizzled::HTON_HIDDEN |
                                      drizzled::HTON_TEMPORARY_NOT_SUPPORTED)
  {
    pthread_mutex_init(&mutex, NULL);
  }

  ~InformationEngine()
  {
    pthread_mutex_destroy(&mutex);
  }

  class Share
  {
  private:
    uint32_t count;
    drizzled::plugin::InfoSchemaTable *table;

  public:
    drizzled::THR_LOCK lock;

    Share(const std::string &in_name) :
      count(1)
    {
      table= drizzled::plugin::InfoSchemaTable::getTable(in_name.c_str());
    }

    ~Share()  {}

    void initThreadLock()
    {
      thr_lock_init(&lock);
    }

    void deinitThreadLock()
    {
      thr_lock_delete(&lock);
    }

    /**
     * Increment the counter which tracks how many instances of this share are
     * currently open.
     * @return the new counter value
   */
    uint32_t incUseCount(void) 
    { 
      return ++count; 
    }

    /**
     * Decrement the count which tracks how many instances of this share are
     * currently open.
     * @return the new counter value
   */
    uint32_t decUseCount(void) 
    { 
      return --count; 
    }

    /**
     * @ return the value of the use counter for this share
   */
    uint32_t getUseCount() const
    {
      return count;
    }

    /**
     * @return the table name associated with this share.
   */
    const std::string &getName() const
    {
      return table->getTableName();
    }

    /**
     * Set the I_S table associated with this share.
     * @param[in] name the name of the I_S table
     */
    void setInfoSchemaTable(const std::string &name)
    {
      table= drizzled::plugin::InfoSchemaTable::getTable(name.c_str());
    }

    /**
     * @return the I_S table associated with this share.
     */
    drizzled::plugin::InfoSchemaTable *getInfoSchemaTable()
    {
      return table;
    }
  };

private:
  typedef std::map<const std::string, Share> OpenTables;
  typedef std::pair<const std::string, Share> Record;

  OpenTables open_tables;
  pthread_mutex_t mutex; // Mutext used in getShare() calls

public:

  // Follow Two Methods are for "share"
  Share *getShare(const std::string &name_arg);
  void freeShare(Share *share);


  int doCreateTable(drizzled::Session *,
                    const char *,
                    drizzled::Table&,
                    drizzled::message::Table&)
  {
    return EPERM;
  }

  int doDropTable(drizzled::Session&, const std::string) 
  { 
    return EPERM; 
  }

  virtual drizzled::Cursor *create(drizzled::TableShare &table,
                                   drizzled::memory::Root *mem_root);

  const char **bas_ext() const 
  {
    return information_exts;
  }

  void doGetTableNames(drizzled::CachedDirectory&, 
                       std::string &db, 
                       std::set<std::string> &set_of_names);

  int doGetTableDefinition(drizzled::Session &session,
                           const char *path,
                           const char *db,
                           const char *table_name,
                           const bool is_tmp,
                           drizzled::message::Table *table_proto);

};

#endif /* PLUGIN_INFORMATION_ENGINE_INFORMATION_ENGINE_H */
