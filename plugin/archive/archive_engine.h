/* Copyright (C) 2003 MySQL AB
   Copyright (C) 2010 Brian Aker

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

#include "drizzled/field.h"
#include "drizzled/field/blob.h"
#include "plugin/myisam/myisam.h"
#include "drizzled/table.h"
#include "drizzled/session.h"
#include <drizzled/thr_lock.h>
#include <drizzled/my_hash.h>
#include <drizzled/cursor.h>

#include <fcntl.h>
#include <inttypes.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include <boost/unordered_map.hpp>
#include "drizzled/util/string.h"

#include "azio.h"
#include "plugin/archive/ha_archive.h"



#include <cstdio>
#include <string>
#include <map>

#ifndef PLUGIN_ARCHIVE_ARCHIVE_ENGINE_H
#define PLUGIN_ARCHIVE_ARCHIVE_ENGINE_H

/* The file extension */
#define ARZ ".arz"               // The data file
#define ARN ".ARN"               // Files used during an optimize call

/*
  We just implement one additional file extension.
*/
static const char *ha_archive_exts[] = {
  ARZ,
  NULL
};


class ArchiveEngine : public drizzled::plugin::StorageEngine
{
  typedef boost::unordered_map<std::string, ArchiveShare *, drizzled::util::insensitive_hash, drizzled::util::insensitive_equal_to> ArchiveMap;
  ArchiveMap archive_open_tables;

  /* Variables for archive share methods */
  pthread_mutex_t _mutex;

public:
  ArchiveEngine() :
    drizzled::plugin::StorageEngine("ARCHIVE",
                                    drizzled::HTON_STATS_RECORDS_IS_EXACT |
                                    drizzled::HTON_HAS_RECORDS),
    archive_open_tables()
  {
    pthread_mutex_init(&_mutex, NULL);
    table_definition_ext= ARZ;
  }
  ~ArchiveEngine()
  {
    pthread_mutex_destroy(&_mutex);
  }

  pthread_mutex_t &mutex()
  {
    return _mutex;
  }

  virtual drizzled::Cursor *create(drizzled::Table &table)
  {
    return new ha_archive(*this, table);
  }

  const char **bas_ext() const {
    return ha_archive_exts;
  }

  int doCreateTable(drizzled::Session &session,
                    drizzled::Table &table_arg,
                    const drizzled::TableIdentifier &identifier,
                    drizzled::message::Table& proto);

  int doGetTableDefinition(drizzled::Session& session,
                           const drizzled::TableIdentifier &identifier,
                           drizzled::message::Table &table_message);

  int doDropTable(drizzled::Session&, const drizzled::TableIdentifier &identifier);

  ArchiveShare *findOpenTable(const std::string table_name);
  void addOpenTable(const std::string &table_name, ArchiveShare *);
  void deleteOpenTable(const std::string &table_name);

  uint32_t max_supported_keys()          const { return 1; }
  uint32_t max_supported_key_length()    const { return sizeof(uint64_t); }
  uint32_t max_supported_key_part_length() const { return sizeof(uint64_t); }

  uint32_t index_flags(enum  drizzled::ha_key_alg) const
  {
    return HA_ONLY_WHOLE_INDEX;
  }

  bool doDoesTableExist(drizzled::Session&, const drizzled::TableIdentifier &identifier);
  int doRenameTable(drizzled::Session&, const drizzled::TableIdentifier &from, const drizzled::TableIdentifier &to);

  void doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                             const drizzled::SchemaIdentifier &schema_identifier,
                             drizzled::TableIdentifier::vector &set_of_identifiers);
};

#endif /* PLUGIN_ARCHIVE_ARCHIVE_ENGINE_H */
