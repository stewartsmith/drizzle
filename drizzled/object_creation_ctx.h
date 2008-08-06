/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef DRIZZLE_SERVER_OBJECT_CREATION_CTX_H
#define DRIZZLE_SERVER_OBJECT_CREATION_CTX_H

/**
 Object_creation_ctx -- interface for creation context of database objects
 (views, stored routines, events, triggers). Creation context -- is a set
 of attributes, that should be fixed at the creation time and then be used
 each time the object is parsed or executed.
*/
class Object_creation_ctx
{
public:
  Object_creation_ctx *set_n_backup(THD *thd);

  void restore_env(THD *thd, Object_creation_ctx *backup_ctx);

protected:
  Object_creation_ctx() {}
  virtual Object_creation_ctx *create_backup_ctx(THD *thd) const = 0;

  virtual void change_env(THD *thd) const = 0;

public:
  virtual ~Object_creation_ctx()
  { }
};

/*************************************************************************/

/**
 Default_object_creation_ctx -- default implementation of
 Object_creation_ctx.
*/

class Default_object_creation_ctx : public Object_creation_ctx
{
public:
  const CHARSET_INFO *get_client_cs()
  {
    return m_client_cs;
  }

  const CHARSET_INFO *get_connection_cl()
  {
    return m_connection_cl;
  }

protected:
  Default_object_creation_ctx(THD *thd);

  Default_object_creation_ctx(const CHARSET_INFO * const client_cs,
                              const CHARSET_INFO * const connection_cl);

protected:
  virtual Object_creation_ctx *create_backup_ctx(THD *thd) const;

  virtual void change_env(THD *thd) const;

protected:
  /**
    client_cs stores the value of character_set_client session variable.
    The only character set attribute is used.

    Client character set is included into query context, because we save
    query in the original character set, which is client character set. So,
    in order to parse the query properly we have to switch client character
    set on parsing.
  */
  const CHARSET_INFO *m_client_cs;

  /**
    connection_cl stores the value of collation_connection session
    variable. Both character set and collation attributes are used.

    Connection collation is included into query context, becase it defines
    the character set and collation of text literals in internal
    representation of query (item-objects).
  */
  const CHARSET_INFO *m_connection_cl;
};

#endif /* DRIZZLE_SERVER_OBJECT_CREATION_CTX_H */
