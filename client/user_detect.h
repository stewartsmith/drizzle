/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Andrew Hutchings
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License, or
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

#ifndef CLIENT_USER_DETECT_H
#define CLIENT_USER_DETECT_H

#include <unistd.h>
#include <pwd.h>
#include <cstring>

class UserDetect
{
  public:
    const char* getUser() { return user.empty() ? "" : &user[0]; }

    UserDetect()
    {
      long pw_len= sysconf(_SC_GETPW_R_SIZE_MAX);
      struct passwd pw_struct;
      struct passwd* pw_tmp_struct;
      char *pw_buffer= new char[pw_len];

      if (getpwuid_r(geteuid(), &pw_struct, pw_buffer, pw_len, &pw_tmp_struct) == 0)
        user.assign(pw_struct.pw_name, pw_struct.pw_name + strlen(pw_struct.pw_name) + 1);

      delete[] pw_buffer;
    }

  private:
    std::vector<char> user;
};

#endif /* CLIENT_USER_DETECT_H */
