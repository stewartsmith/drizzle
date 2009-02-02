/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#ifndef LIBDRIZZLECLIENT_PASSWORD_H
#define LIBDRIZZLECLIENT_PASSWORD_H

#include <stdint.h>

/*
 * These functions are used for authentication by client and server.
 */

#ifdef __cplusplus
extern "C" {
#endif

  struct rand_struct {
    unsigned long seed1,seed2,max_value;
    double max_value_dbl;
  };


  void randominit(struct rand_struct *, uint32_t seed1, uint32_t seed2);
  double my_rnd(struct rand_struct *);
  void create_random_string(char *to, unsigned int length,
                            struct rand_struct *rand_st);

  void hash_password(uint32_t *to, const char *password,
                     uint32_t password_len);

  void make_scrambled_password(char *to, const char *password);
  void scramble(char *to, const char *message, const char *password);
  bool check_scramble(const char *reply, const char *message,
                      const unsigned char *hash_stage2);
  void get_salt_from_password(unsigned char *res, const char *password);
  void make_password_from_salt(char *to, const unsigned char *hash_stage2);
  char *octet2hex(char *to, const char *str, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif /* LIBDRIZZLECLIENT_PASSWORD_H */
