/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Pawel Blokus
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

#include "config.h"

#include <boost/shared_ptr.hpp>

#include <cstdlib>
#include <gtest/gtest.h>
#include <drizzled/item.h>
#include <drizzled/session.h>
#include <drizzled/sql_string.h>
#include <drizzled/temporal_interval.h>
#include <drizzled/plugin/listen.h>

using namespace drizzled;
/*
class ItemStub : public Item
{
  ItemStub(Session *fake_session) : Item(fake_session, this)
  {
    string_to_return = NULL;
    int_to_return = 0;
  }
  
  public:
    String *string_to_return;
    int int_to_return;
    
    static boost::shared_ptr<ItemStub> get_item_stub(Session *fake_session)
    {
      boost::shared_ptr<ItemStub> result(new ItemStub(fake_session));
      return result;
    }
    
    virtual enum Type type() const { return FIELD_ITEM; };
    virtual double val_real() { return 0; };
    virtual int64_t val_int() { return int_to_return; };
    virtual String *val_str(String *str)
    {
      (void) str;
      return string_to_return;
    };
    virtual my_decimal *val_decimal(my_decimal *decimal_buffer)
    {
      (void) decimal_buffer;
      return NULL;
    };
};

class TemporalIntervalTest : public ::testing::Test
{
  protected:
    String string_to_return;
    String buffer;
    bool result;
    boost::shared_ptr<ItemStub> item;
    TemporalInterval interval;
    Session *fake_session;

    plugin::Client *fake_client;
    
    virtual void SetUp()
    {
      fake_client= plugin::Listen::getNullClient();
      fake_session = new Session(fake_client);
      item = ItemStub::get_item_stub(fake_session);

      string_to_return.alloc(100);//TODO: some reasonable size here
      buffer.alloc(100);
      item->string_to_return = &string_to_return;
      item->int_to_return = 0;
      item->null_value = false;
    }
};

TEST_F(TemporalIntervalTest, initFromItem_intervalWeek)
{
  char string[] = "aaa";
  item->string_to_return->set_ascii(string, strlen(string));
  item->int_to_return = 30;

  interval.initFromItem(item.get(), INTERVAL_WEEK, &buffer);

  ASSERT_EQ(210, interval.get_day());
}
*/