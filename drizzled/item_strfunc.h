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


#include <drizzled/functions/str/concat.h>
#include <drizzled/functions/str/replace.h>
#include <drizzled/functions/str/reverse.h>

class Item_func_insert :public Item_str_func
{
  String tmp_value;
public:
  Item_func_insert(Item *org,Item *start,Item *length,Item *new_str)
    :Item_str_func(org,start,length,new_str) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "insert"; }
};


class Item_str_conv :public Item_str_func
{
protected:
  uint32_t multiply;
  my_charset_conv_case converter;
  String tmp_value;
public:
  Item_str_conv(Item *item) :Item_str_func(item) {}
  String *val_str(String *);
};


class Item_func_lcase :public Item_str_conv
{
public:
  Item_func_lcase(Item *item) :Item_str_conv(item) {}
  const char *func_name() const { return "lcase"; }
  void fix_length_and_dec();
};

class Item_func_ucase :public Item_str_conv
{
public:
  Item_func_ucase(Item *item) :Item_str_conv(item) {}
  const char *func_name() const { return "ucase"; }
  void fix_length_and_dec();
};


class Item_func_left :public Item_str_func
{
  String tmp_value;
public:
  Item_func_left(Item *a,Item *b) :Item_str_func(a,b) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "left"; }
};


class Item_func_right :public Item_str_func
{
  String tmp_value;
public:
  Item_func_right(Item *a,Item *b) :Item_str_func(a,b) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "right"; }
};


class Item_func_substr :public Item_str_func
{
  String tmp_value;
public:
  Item_func_substr(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_func_substr(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "substr"; }
};


class Item_func_substr_index :public Item_str_func
{
  String tmp_value;
public:
  Item_func_substr_index(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "substring_index"; }
};


class Item_func_trim :public Item_str_func
{
protected:
  String tmp_value;
  String remove;
public:
  Item_func_trim(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_func_trim(Item *a) :Item_str_func(a) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "trim"; }
  virtual void print(String *str, enum_query_type query_type);
  virtual const char *mode_name() const { return "both"; }
};


class Item_func_ltrim :public Item_func_trim
{
public:
  Item_func_ltrim(Item *a,Item *b) :Item_func_trim(a,b) {}
  Item_func_ltrim(Item *a) :Item_func_trim(a) {}
  String *val_str(String *);
  const char *func_name() const { return "ltrim"; }
  const char *mode_name() const { return "leading"; }
};


class Item_func_rtrim :public Item_func_trim
{
public:
  Item_func_rtrim(Item *a,Item *b) :Item_func_trim(a,b) {}
  Item_func_rtrim(Item *a) :Item_func_trim(a) {}
  String *val_str(String *);
  const char *func_name() const { return "rtrim"; }
  const char *mode_name() const { return "trailing"; }
};


class Item_func_sysconst :public Item_str_func
{
public:
  Item_func_sysconst()
  { collation.set(system_charset_info,DERIVATION_SYSCONST); }
  Item *safe_charset_converter(const CHARSET_INFO * const tocs);
  /*
    Used to create correct Item name in new converted item in
    safe_charset_converter, return string representation of this function
    call
  */
  virtual const char *fully_qualified_func_name() const = 0;
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};


class Item_func_database :public Item_func_sysconst
{
public:
  Item_func_database() :Item_func_sysconst() {}
  String *val_str(String *);
  void fix_length_and_dec()
  {
    max_length= MAX_FIELD_NAME * system_charset_info->mbmaxlen;
    maybe_null=1;
  }
  const char *func_name() const { return "database"; }
  const char *fully_qualified_func_name() const { return "database()"; }
};


class Item_func_user :public Item_func_sysconst
{
protected:
  bool init (const char *user, const char *host);

public:
  Item_func_user()
  {
    str_value.set("", 0, system_charset_info);
  }
  String *val_str(String *)
  {
    assert(fixed == 1);
    return (null_value ? 0 : &str_value);
  }
  bool fix_fields(Session *session, Item **ref);
  void fix_length_and_dec()
  {
    max_length= (USERNAME_CHAR_LENGTH + HOSTNAME_LENGTH + 1) *
                system_charset_info->mbmaxlen;
  }
  const char *func_name() const { return "user"; }
  const char *fully_qualified_func_name() const { return "user()"; }
  int save_in_field(Field *field,
                    bool no_conversions __attribute__((unused)))
  {
    return save_str_value_in_field(field, &str_value);
  }
};


class Item_func_current_user :public Item_func_user
{
  Name_resolution_context *context;

public:
  Item_func_current_user(Name_resolution_context *context_arg)
    : context(context_arg) {}
  bool fix_fields(Session *session, Item **ref);
  const char *func_name() const { return "current_user"; }
  const char *fully_qualified_func_name() const { return "current_user()"; }
};


class Item_func_soundex :public Item_str_func
{
  String tmp_value;
public:
  Item_func_soundex(Item *a) :Item_str_func(a) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "soundex"; }
};


class Item_func_elt :public Item_str_func
{
public:
  Item_func_elt(List<Item> &list) :Item_str_func(list) {}
  double val_real();
  int64_t val_int();
  String *val_str(String *str);
  void fix_length_and_dec();
  const char *func_name() const { return "elt"; }
};


class Item_func_make_set :public Item_str_func
{
  Item *item;
  String tmp_str;

public:
  Item_func_make_set(Item *a,List<Item> &list) :Item_str_func(list),item(a) {}
  String *val_str(String *str);
  bool fix_fields(Session *session, Item **ref)
  {
    assert(fixed == 0);
    return ((!item->fixed && item->fix_fields(session, &item)) ||
	    item->check_cols(1) ||
	    Item_func::fix_fields(session, ref));
  }
  void split_sum_func(Session *session, Item **ref_pointer_array, List<Item> &fields);
  void fix_length_and_dec();
  void update_used_tables();
  const char *func_name() const { return "make_set"; }

  bool walk(Item_processor processor, bool walk_subquery, unsigned char *arg)
  {
    return item->walk(processor, walk_subquery, arg) ||
      Item_str_func::walk(processor, walk_subquery, arg);
  }
  Item *transform(Item_transformer transformer, unsigned char *arg);
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_format :public Item_str_func
{
  String tmp_str;
public:
  Item_func_format(Item *org, Item *dec);
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "format"; }
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_char :public Item_str_func
{
public:
  Item_func_char(List<Item> &list) :Item_str_func(list)
  { collation.set(&my_charset_bin); }
  Item_func_char(List<Item> &list, const CHARSET_INFO * const cs) :Item_str_func(list)
  { collation.set(cs); }  
  String *val_str(String *);
  void fix_length_and_dec() 
  {
    max_length= arg_count * 4;
  }
  const char *func_name() const { return "char"; }
};


class Item_func_repeat :public Item_str_func
{
  String tmp_value;
public:
  Item_func_repeat(Item *arg1,Item *arg2) :Item_str_func(arg1,arg2) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "repeat"; }
};


class Item_func_rpad :public Item_str_func
{
  String tmp_value, rpad_str;
public:
  Item_func_rpad(Item *arg1,Item *arg2,Item *arg3)
    :Item_str_func(arg1,arg2,arg3) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "rpad"; }
};


class Item_func_lpad :public Item_str_func
{
  String tmp_value, lpad_str;
public:
  Item_func_lpad(Item *arg1,Item *arg2,Item *arg3)
    :Item_str_func(arg1,arg2,arg3) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "lpad"; }
};


class Item_func_conv :public Item_str_func
{
public:
  Item_func_conv(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  const char *func_name() const { return "conv"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(default_charset());
    max_length=64;
    maybe_null= 1;
  }
};


class Item_func_hex :public Item_str_func
{
  String tmp_value;
public:
  Item_func_hex(Item *a) :Item_str_func(a) {}
  const char *func_name() const { return "hex"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(default_charset());
    decimals=0;
    max_length=args[0]->max_length*2*collation.collation->mbmaxlen;
  }
};

class Item_func_unhex :public Item_str_func
{
  String tmp_value;
public:
  Item_func_unhex(Item *a) :Item_str_func(a) 
  { 
    /* there can be bad hex strings */
    maybe_null= 1; 
  }
  const char *func_name() const { return "unhex"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=(1+args[0]->max_length)/2;
  }
};


class Item_func_binary :public Item_str_func
{
public:
  Item_func_binary(Item *a) :Item_str_func(a) {}
  String *val_str(String *a)
  {
    assert(fixed == 1);
    String *tmp=args[0]->val_str(a);
    null_value=args[0]->null_value;
    if (tmp)
      tmp->set_charset(&my_charset_bin);
    return tmp;
  }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    max_length=args[0]->max_length;
  }
  virtual void print(String *str, enum_query_type query_type);
  const char *func_name() const { return "cast_as_binary"; }
};


class Item_load_file :public Item_str_func
{
  String tmp_value;
public:
  Item_load_file(Item *a) :Item_str_func(a) {}
  String *val_str(String *);
  const char *func_name() const { return "load_file"; }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
    maybe_null=1;
    max_length=MAX_BLOB_WIDTH;
  }
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};


class Item_func_export_set: public Item_str_func
{
 public:
  Item_func_export_set(Item *a,Item *b,Item* c) :Item_str_func(a,b,c) {}
  Item_func_export_set(Item *a,Item *b,Item* c,Item* d) :Item_str_func(a,b,c,d) {}
  Item_func_export_set(Item *a,Item *b,Item* c,Item* d,Item* e) :Item_str_func(a,b,c,d,e) {}
  String  *val_str(String *str);
  void fix_length_and_dec();
  const char *func_name() const { return "export_set"; }
};

class Item_func_quote :public Item_str_func
{
  String tmp_value;
public:
  Item_func_quote(Item *a) :Item_str_func(a) {}
  const char *func_name() const { return "quote"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(args[0]->collation);
    max_length= args[0]->max_length * 2 + 2;
  }
};

class Item_func_conv_charset :public Item_str_func
{
  bool use_cached_value;
public:
  bool safe;
  const CHARSET_INFO *conv_charset; // keep it public
  Item_func_conv_charset(Item *a, const CHARSET_INFO * const cs) :Item_str_func(a) 
  { conv_charset= cs; use_cached_value= 0; safe= 0; }
  Item_func_conv_charset(Item *a, const CHARSET_INFO * const cs, bool cache_if_const) 
    :Item_str_func(a) 
  {
    assert(args[0]->fixed);
    conv_charset= cs;
    if (cache_if_const && args[0]->const_item())
    {
      uint32_t errors= 0;
      String tmp, *str= args[0]->val_str(&tmp);
      if (!str || str_value.copy(str->ptr(), str->length(),
                                 str->charset(), conv_charset, &errors))
        null_value= 1;
      use_cached_value= 1;
      str_value.mark_as_const();
      safe= (errors == 0);
    }
    else
    {
      use_cached_value= 0;
      /*
        Conversion from and to "binary" is safe.
        Conversion to Unicode is safe.
        Other kind of conversions are potentially lossy.
      */
      safe= (args[0]->collation.collation == &my_charset_bin ||
             cs == &my_charset_bin ||
             (cs->state & MY_CS_UNICODE));
    }
  }
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "convert"; }
  virtual void print(String *str, enum_query_type query_type);
};

class Item_func_set_collation :public Item_str_func
{
public:
  Item_func_set_collation(Item *a, Item *b) :Item_str_func(a,b) {};
  String *val_str(String *);
  void fix_length_and_dec();
  bool eq(const Item *item, bool binary_cmp) const;
  const char *func_name() const { return "collate"; }
  enum Functype functype() const { return COLLATE_FUNC; }
  virtual void print(String *str, enum_query_type query_type);
  Item_field *filed_for_view_update()
  {
    /* this function is transparent for view updating */
    return args[0]->filed_for_view_update();
  }
};

class Item_func_charset :public Item_str_func
{
public:
  Item_func_charset(Item *a) :Item_str_func(a) {}
  String *val_str(String *);
  const char *func_name() const { return "charset"; }
  void fix_length_and_dec()
  {
     collation.set(system_charset_info);
     max_length= 64 * collation.collation->mbmaxlen; // should be enough
     maybe_null= 0;
  };
  table_map not_null_tables() const { return 0; }
};

class Item_func_collation :public Item_str_func
{
public:
  Item_func_collation(Item *a) :Item_str_func(a) {}
  String *val_str(String *);
  const char *func_name() const { return "collation"; }
  void fix_length_and_dec()
  {
     collation.set(system_charset_info);
     max_length= 64 * collation.collation->mbmaxlen; // should be enough
     maybe_null= 0;
  };
  table_map not_null_tables() const { return 0; }
};


class Item_func_weight_string :public Item_str_func
{
  String tmp_value;
  uint32_t flags;
  uint32_t nweights;
public:
  Item_func_weight_string(Item *a, uint32_t nweights_arg, uint32_t flags_arg)
  :Item_str_func(a) { nweights= nweights_arg; flags= flags_arg; }
  const char *func_name() const { return "weight_string"; }
  String *val_str(String *);
  void fix_length_and_dec();
  /*
    TODO: Currently this Item is not allowed for virtual columns
    only due to a bug in generating virtual column value.
  */
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};

#define UUID_LENGTH (8+1+4+1+4+1+4+1+12)
class Item_func_uuid: public Item_str_func
{
public:
  Item_func_uuid(): Item_str_func() {}
  void fix_length_and_dec() {
    collation.set(system_charset_info);
    /*
       NOTE! uuid() should be changed to use 'ascii'
       charset when hex(), format(), md5(), etc, and implicit
       number-to-string conversion will use 'ascii'
    */
    max_length= UUID_LENGTH * system_charset_info->mbmaxlen;
  }
  const char *func_name() const{ return "uuid"; }
  String *val_str(String *);
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};

