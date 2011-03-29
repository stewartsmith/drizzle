/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#pragma once

#include <boost/dynamic_bitset.hpp>

#include <drizzled/field.h>

namespace drizzled {

typedef struct st_key_part KEY_PART;

namespace optimizer {

class RorScanInfo
{
public:
  RorScanInfo()
    :
      idx(0),
      keynr(0),
      records(0), 
      sel_arg(NULL),
      covered_fields(0),
      covered_fields_size(0),
      used_fields_covered(0),
      key_rec_length(0),
      index_read_cost(0.0),
      first_uncovered_field(0),
      key_components(0)
  {}

  boost::dynamic_bitset<> bitsToBitset() const;

  void subtractBitset(const boost::dynamic_bitset<>& in_bitset);

  uint32_t findFirstNotSet() const; 

  size_t getBitCount() const;

  uint32_t      idx;      /* # of used key in param->keys */
  uint32_t      keynr;    /* # of used key in table */
  ha_rows   records;  /* estimate of # records this scan will return */

  /* Set of intervals over key fields that will be used for row retrieval. */
  optimizer::SEL_ARG   *sel_arg;

  /* Fields used in the query and covered by this ROR scan. */
  uint64_t covered_fields;
  size_t covered_fields_size;
  uint32_t      used_fields_covered; /* # of set bits in covered_fields */
  int       key_rec_length; /* length of key record (including rowid) */

  /*
    Cost of reading all index records with values in sel_arg intervals set
    (assuming there is no need to access full table records)
  */
  double    index_read_cost;
  uint32_t      first_uncovered_field; /* first unused bit in covered_fields */
  uint32_t      key_components; /* # of parts in the key */
};

class RangeParameter
{
public:

  RangeParameter()
    :
      session(NULL),
      table(NULL),
      cond(NULL),
      prev_tables(),
      read_tables(),
      current_table(),
      key_parts(NULL),
      key_parts_end(NULL),
      mem_root(NULL),
      old_root(NULL),
      keys(0),
      using_real_indexes(false),
      remove_jump_scans(false),
      alloced_sel_args(0),
      force_default_mrr(false)
  {}

  Session	*session;   /* Current thread handle */
  Table *table; /* Table being analyzed */
  COND *cond;   /* Used inside get_mm_tree(). */
  table_map prev_tables;
  table_map read_tables;
  table_map current_table; /* Bit of the table being analyzed */

  /* Array of parts of all keys for which range analysis is performed */
  KEY_PART *key_parts;
  KEY_PART *key_parts_end;
  memory::Root *mem_root; /* Memory that will be freed when range analysis completes */
  memory::Root *old_root; /* Memory that will last until the query end */
  /*
    Number of indexes used in range analysis (In SEL_TREE::keys only first
    #keys elements are not empty)
  */
  uint32_t keys;

  /*
    If true, the index descriptions describe real indexes (and it is ok to
    call field->optimize_range(real_keynr[...], ...).
    Otherwise index description describes fake indexes.
  */
  bool using_real_indexes;

  bool remove_jump_scans;

  /*
    used_key_no -> table_key_no translation table. Only makes sense if
    using_real_indexes==true
  */
  uint32_t real_keynr[MAX_KEY];
  /* Number of SEL_ARG objects allocated by optimizer::SEL_ARG::clone_tree operations */
  uint32_t alloced_sel_args;
  bool force_default_mrr;
};

class Parameter : public RangeParameter
{
public:

  Parameter()
    :
      RangeParameter(),
      max_key_part(0),
      range_count(0),
      quick(false),
      needed_fields(),
      tmp_covered_fields(),
      needed_reg(NULL),
      imerge_cost_buff(NULL),
      imerge_cost_buff_size(0),
      is_ror_scan(false),
      n_ranges(0)
  {}

  KEY_PART *key[MAX_KEY]; /* First key parts of keys used in the query */
  uint32_t max_key_part;
  /* Number of ranges in the last checked tree->key */
  uint32_t range_count;
  unsigned char min_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH];
  unsigned char max_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH];
  bool quick; // Don't calulate possible keys

  boost::dynamic_bitset<> needed_fields;    /* bitmask of fields needed by the query */
  boost::dynamic_bitset<> tmp_covered_fields;

  key_map *needed_reg;        /* ptr to SqlSelect::needed_reg */

  uint32_t *imerge_cost_buff;     /* buffer for index_merge cost estimates */
  uint32_t imerge_cost_buff_size; /* size of the buffer */

  /* true if last checked tree->key can be used for ROR-scan */
  bool is_ror_scan;
  /* Number of ranges in the last checked tree->key */
  uint32_t n_ranges;
};

} /* namespace optimizer */

} /* namespace drizzled */

