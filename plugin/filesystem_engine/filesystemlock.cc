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

#include "config.h"
#include <pthread.h>
#include <assert.h>
#include "filesystemlock.h"

FilesystemLock::FilesystemLock()
  : scanner_count(0),
    updater_count(0)
{
  pthread_cond_init(&condition, NULL);
  pthread_mutex_init(&mutex, NULL);
}

FilesystemLock::~FilesystemLock()
{
  pthread_cond_destroy(&condition);
  pthread_mutex_destroy(&mutex);
}

void FilesystemLock::scan_begin()
{
  pthread_mutex_lock(&mutex);
  while (true)
  {
    if (updater_count == 0)
    {
      scanner_count++;
      pthread_mutex_unlock(&mutex);
      return;
    }
    pthread_cond_wait(&condition, &mutex);
  }
}

void FilesystemLock::scan_end() {
  pthread_mutex_lock(&mutex);
  scanner_count--;
  assert(scanner_count >= 0);
  if (scanner_count == 0)
    pthread_cond_broadcast(&condition);
  pthread_mutex_unlock(&mutex);
}

void FilesystemLock::scan_update_begin()
{
  pthread_mutex_lock(&mutex);
  while (true)
  {
    if (scanner_count == 0 && updater_count == 0)
    {
      scanner_count++;
      updater_count++;
      pthread_mutex_unlock(&mutex);
      return;
    }
    pthread_cond_wait(&condition, &mutex);
  }
}

void FilesystemLock::scan_update_end()
{
  pthread_mutex_lock(&mutex);
  scanner_count--;
  updater_count--;
  assert(scanner_count >= 0 && updater_count >= 0);

  pthread_cond_broadcast(&condition);
  pthread_mutex_unlock(&mutex);
}
