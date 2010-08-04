/*
  Copyright (C) 2010 Zimin

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef PLUGIN_FILESYSTEM_ENGINE_FILESYSTEMLOCK_H
#define PLUGIN_FILESYSTEM_ENGINE_FILESYSTEMLOCK_H

#include <pthread.h>

class FilesystemLock
{
public:
  FilesystemLock();
  ~FilesystemLock();

  void scan_begin();
  void scan_end();
  void scan_update_begin();
  void scan_update_end();
private:
  pthread_cond_t condition;
  pthread_mutex_t mutex;
  int scanner_count;
  int updater_count;
};

class Guard
{
public:
  Guard(pthread_mutex_t& mutex) : mutex_(&mutex)
  {
    pthread_mutex_lock(mutex_);
  }

  ~Guard()
  {
    if (mutex_)
    {
      pthread_mutex_unlock(mutex_);
    }
  }
private:
  pthread_mutex_t *mutex_;
};

#endif /* PLUGIN_FILESYSTEM_ENGINE_FILESYSTEMLOCK_H */
