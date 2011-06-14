/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Joseph Daly <skinny.moey@gmail.com> 
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

#pragma once

#include <drizzled/plugin/table_function.h>
#include <drizzled/field.h>

#include "trx0i_s.h"
#include "trx0trx.h" /* for TRX_QUE_STATE_STR_MAX_LEN */
#include "buf0buddy.h" /* for i_s_cmpmem */
#include "buf0buf.h" /* for buf_pool and PAGE_ZIP_MIN_SIZE */
#include "ha_prototypes.h" /* for innobase_convert_name() */
#include "srv0start.h" /* for srv_was_started */
#include "btr0pcur.h"	/* for file sys_tables related info. */
#include "btr0types.h"
#include "dict0load.h"	/* for file sys_tables related info. */
#include "dict0mem.h"
#include "dict0types.h"

class InnodbSysTablesTool : public drizzled::plugin::TableFunction
{
public:
  InnodbSysTablesTool();
  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);

    bool populate();
  private:
    btr_pcur_t	pcur;
    const rec_t* rec;
    mem_heap_t*	heap;
    mtr_t mtr;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

class InnodbSysTableStatsTool : public drizzled::plugin::TableFunction
{
public:
  InnodbSysTableStatsTool();
  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);

    bool populate();
  private:
    btr_pcur_t	pcur;
    const rec_t* rec;
    mem_heap_t*	heap;
    mtr_t mtr;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

class InnodbSysIndexesTool : public drizzled::plugin::TableFunction
{
public:
  InnodbSysIndexesTool();
  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);

    bool populate();
  private:
    btr_pcur_t	pcur;
    const rec_t* rec;
    mem_heap_t*	heap;
    mtr_t mtr;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

class InnodbSysColumnsTool : public drizzled::plugin::TableFunction
{
public:
  InnodbSysColumnsTool();
  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);

    bool populate();
  private:
    btr_pcur_t	pcur;
    const rec_t* rec;
    mem_heap_t*	heap;
    mtr_t mtr;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

class InnodbSysFieldsTool : public drizzled::plugin::TableFunction
{
public:
  InnodbSysFieldsTool();
  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);

    bool populate();
  private:
    btr_pcur_t pcur;
    const rec_t* rec;
    mem_heap_t*	heap;
    index_id_t last_id;
    mtr_t mtr;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

class InnodbSysForeignTool : public drizzled::plugin::TableFunction
{
public:
  InnodbSysForeignTool();
  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);

    bool populate();
  private:
    btr_pcur_t pcur;
    const rec_t* rec;
    mem_heap_t*	heap;
    mtr_t mtr;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

class InnodbSysForeignColsTool : public drizzled::plugin::TableFunction
{
public:
  InnodbSysForeignColsTool();
  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg);

    bool populate();
  private:
    btr_pcur_t pcur;
    const rec_t* rec;
    mem_heap_t*	heap;
    mtr_t mtr;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

class CmpTool : public drizzled::plugin::TableFunction
{
public:

  CmpTool(bool reset);

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg, bool outer_reset);
                        
    bool populate();
  private:
    uint32_t record_number;
    bool inner_reset;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg, outer_reset);
  }
private:
  bool outer_reset; 
};

class CmpmemTool : public drizzled::plugin::TableFunction
{
public:

  CmpmemTool(bool reset);

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg, bool outer_reset);

    ~Generator();

    bool populate();
  private:
    uint32_t record_number;
    bool inner_reset;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg, outer_reset);
  }
private:
  bool outer_reset;
};

class InnodbTrxTool : public drizzled::plugin::TableFunction
{
public:

  InnodbTrxTool(const char* in_table_name);

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg, const char* in_table_name);

    ~Generator();

    bool populate();
  private:
    void populate_innodb_trx();
    void populate_innodb_locks();
    void populate_innodb_lock_waits();

  private:
    uint32_t record_number;
    uint32_t number_rows;
    const char* table_name;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg, table_name);
  }
private:
  const char* table_name;
};

