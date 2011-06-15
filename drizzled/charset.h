/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

/*
  Header File that defines all the charset declarations being used in Drizzle source code
*/

#pragma once

#include <sys/types.h>
#include <cstddef>
#include <drizzled/visibility.h>
#include <drizzled/definitions.h>

namespace drizzled
{

#define MY_CS_NAME_SIZE			32
#define MY_CS_CTYPE_TABLE_SIZE		257
#define MY_CS_TO_LOWER_TABLE_SIZE	256
#define MY_CS_TO_UPPER_TABLE_SIZE	256
#define MY_CS_SORT_ORDER_TABLE_SIZE	256
#define MY_CS_TO_UNI_TABLE_SIZE		256
#define CHARSET_DIR	"charsets/"
#define my_wc_t unsigned long
/* wm_wc and wc_mb return codes */
#define MY_CS_ILSEQ	0     /* Wrong by sequence: wb_wc                   */
#define MY_CS_ILUNI	0     /* Cannot encode Unicode to charset: wc_mb    */
#define MY_CS_TOOSMALL  -101  /* Need at least one byte:    wc_mb and mb_wc */
#define MY_CS_TOOSMALL2 -102  /* Need at least two bytes:   wc_mb and mb_wc */
#define MY_CS_TOOSMALL3 -103  /* Need at least three bytes: wc_mb and mb_wc */
/* These following three are currently not really used */
#define MY_CS_TOOSMALL4 -104  /* Need at least 4 bytes: wc_mb and mb_wc */
#define MY_CS_TOOSMALL5 -105  /* Need at least 5 bytes: wc_mb and mb_wc */
#define MY_CS_TOOSMALL6 -106  /* Need at least 6 bytes: wc_mb and mb_wc */
#define MY_SEQ_INTTAIL	1
#define MY_SEQ_SPACES	2
#define MY_CS_COMPILED  1      /* compiled-in sets               */
#define MY_CS_CONFIG    2      /* sets that have a *.conf file   */
#define MY_CS_INDEX     4      /* sets listed in the Index file  */
#define MY_CS_LOADED    8      /* sets that are currently loaded */
#define MY_CS_BINSORT	16     /* if binary sort order           */
#define MY_CS_PRIMARY	32     /* if primary collation           */
#define MY_CS_STRNXFRM	64     /* if strnxfrm is used for sort   */
#define MY_CS_UNICODE	128    /* is a charset is full unicode   */
#define MY_CS_READY	256    /* if a charset is initialized    */
#define MY_CS_AVAILABLE	512    /* If either compiled-in or loaded*/
#define MY_CS_CSSORT	1024   /* if case sensitive sort order   */
#define MY_CS_HIDDEN	2048   /* don't display in SHOW          */
#define MY_CS_NONASCII  8192   /* if not ASCII-compatible        */
#define MY_CHARSET_UNDEFINED 0
/* Flags for strxfrm */
#define MY_STRXFRM_LEVEL1          0x00000001 /* for primary weights   */
#define MY_STRXFRM_LEVEL2          0x00000002 /* for secondary weights */
#define MY_STRXFRM_LEVEL3          0x00000004 /* for tertiary weights  */
#define MY_STRXFRM_LEVEL4          0x00000008 /* fourth level weights  */
#define MY_STRXFRM_LEVEL5          0x00000010 /* fifth level weights   */
#define MY_STRXFRM_LEVEL6          0x00000020 /* sixth level weights   */
#define MY_STRXFRM_LEVEL_ALL       0x0000003F /* Bit OR for the above six */
#define MY_STRXFRM_NLEVELS         6          /* Number of possible levels*/
#define MY_STRXFRM_PAD_WITH_SPACE  0x00000040 /* if pad result with spaces */
#define MY_STRXFRM_UNUSED_00000080 0x00000080 /* for future extensions     */
#define MY_STRXFRM_DESC_LEVEL1     0x00000100 /* if desc order for level1 */
#define MY_STRXFRM_DESC_LEVEL2     0x00000200 /* if desc order for level2 */
#define MY_STRXFRM_DESC_LEVEL3     0x00000300 /* if desc order for level3 */
#define MY_STRXFRM_DESC_LEVEL4     0x00000800 /* if desc order for level4 */
#define MY_STRXFRM_DESC_LEVEL5     0x00001000 /* if desc order for level5 */
#define MY_STRXFRM_DESC_LEVEL6     0x00002000 /* if desc order for level6 */
#define MY_STRXFRM_DESC_SHIFT      8
#define MY_STRXFRM_UNUSED_00004000 0x00004000 /* for future extensions     */
#define MY_STRXFRM_UNUSED_00008000 0x00008000 /* for future extensions     */
#define MY_STRXFRM_REVERSE_LEVEL1  0x00010000 /* if reverse order for level1 */
#define MY_STRXFRM_REVERSE_LEVEL2  0x00020000 /* if reverse order for level2 */
#define MY_STRXFRM_REVERSE_LEVEL3  0x00040000 /* if reverse order for level3 */
#define MY_STRXFRM_REVERSE_LEVEL4  0x00080000 /* if reverse order for level4 */
#define MY_STRXFRM_REVERSE_LEVEL5  0x00100000 /* if reverse order for level5 */
#define MY_STRXFRM_REVERSE_LEVEL6  0x00200000 /* if reverse order for level6 */
#define MY_STRXFRM_REVERSE_SHIFT   16
#define ILLEGAL_CHARSET_INFO_NUMBER (UINT32_MAX)
#define MY_UTF8MB4                 "utf8"
#define my_charset_utf8_general_ci ::drizzled::my_charset_utf8mb4_general_ci
#define my_charset_utf8_bin        ::drizzled::my_charset_utf8mb4_bin
#define	_MY_U	01	/* Upper case */
#define	_MY_L	02	/* Lower case */
#define	_MY_NMR	04	/* Numeral (digit) */
#define	_MY_SPC	010	/* Spacing character */
#define	_MY_PNT	020	/* Punctuation */
#define	_MY_CTR	040	/* Control character */
#define	_MY_B	0100	/* Blank */
#define	_MY_X	0200	/* heXadecimal digit */

/* Some typedef to make it easy for C++ to make function pointers */
typedef int (*my_charset_conv_mb_wc)(const struct charset_info_st * const, my_wc_t *,
                                     const unsigned char *, const unsigned char *);
typedef int (*my_charset_conv_wc_mb)(const struct charset_info_st * const, my_wc_t,
                                     unsigned char *, unsigned char *);
typedef size_t (*my_charset_conv_case)(const struct charset_info_st * const,
                                       char *, size_t, char *, size_t);
struct charset_info_st;
typedef struct unicase_info_st
{
  uint16_t toupper;
  uint16_t tolower;
  uint16_t sort;
} MY_UNICASE_INFO;

typedef struct uni_ctype_st
{
  unsigned char  pctype;
  unsigned char  *ctype;
} MY_UNI_CTYPE;

/* A helper function for "need at least n bytes" */
inline static int my_cs_toosmalln(int n)
{
  return -100-n;
}

typedef struct my_uni_idx_st
{
  uint16_t from;
  uint16_t to;
  unsigned char  *tab;
} MY_UNI_IDX;

typedef struct
{
  uint32_t beg;
  uint32_t end;
  uint32_t mb_len;
} my_match_t;

enum my_lex_states
{
  MY_LEX_START, MY_LEX_CHAR, MY_LEX_IDENT,
  MY_LEX_IDENT_SEP, MY_LEX_IDENT_START,
  MY_LEX_REAL, MY_LEX_HEX_NUMBER, MY_LEX_BIN_NUMBER,
  MY_LEX_CMP_OP, MY_LEX_LONG_CMP_OP, MY_LEX_STRING, MY_LEX_COMMENT, MY_LEX_END,
  MY_LEX_OPERATOR_OR_IDENT, MY_LEX_NUMBER_IDENT, MY_LEX_INT_OR_REAL,
  MY_LEX_REAL_OR_POINT, MY_LEX_BOOL, MY_LEX_EOL, MY_LEX_ESCAPE,
  MY_LEX_LONG_COMMENT, MY_LEX_END_LONG_COMMENT, MY_LEX_SEMICOLON,
  MY_LEX_SET_VAR, MY_LEX_USER_END, MY_LEX_HOSTNAME, MY_LEX_SKIP,
  MY_LEX_USER_VARIABLE_DELIMITER, MY_LEX_SYSTEM_VAR,
  MY_LEX_IDENT_OR_KEYWORD,
  MY_LEX_IDENT_OR_HEX, MY_LEX_IDENT_OR_BIN,
  MY_LEX_STRING_OR_DELIMITER
};

struct charset_info_st;

/* See strings/charset_info_st.txt for information about this structure  */
typedef struct my_collation_handler_st
{
  bool (*init)(struct charset_info_st *, unsigned char *(*alloc)(size_t));
  /* Collation routines */
  int     (*strnncoll)(const struct charset_info_st * const,
		       const unsigned char *, size_t, const unsigned char *, size_t, bool);
  int     (*strnncollsp)(const struct charset_info_st * const,
                         const unsigned char *, size_t, const unsigned char *, size_t,
                         bool diff_if_only_endspace_difference);
  size_t  (*strnxfrm)(const struct charset_info_st * const,
                      unsigned char *dst, size_t dstlen, uint32_t nweights,
                      const unsigned char *src, size_t srclen, uint32_t flags);
  size_t    (*strnxfrmlen)(const struct charset_info_st * const, size_t);
  bool (*like_range)(const struct charset_info_st * const,
                        const char *s, size_t s_length,
                        char escape, char w_one, char w_many,
                        size_t res_length,
                        char *min_str, char *max_str,
                        size_t *min_len, size_t *max_len);
  int     (*wildcmp)(const struct charset_info_st * const,
  		     const char *str,const char *str_end,
                     const char *wildstr,const char *wildend,
                     int escape,int w_one, int w_many);

  int  (*strcasecmp)(const struct charset_info_st * const, const char *, const char *);

  uint32_t (*instr)(const struct charset_info_st * const,
                const char *b, size_t b_length,
                const char *s, size_t s_length,
                my_match_t *match, uint32_t nmatch);

  /* Hash calculation */
  void (*hash_sort)(const struct charset_info_st *cs, const unsigned char *key, size_t len,
                    uint32_t *nr1, uint32_t *nr2);
  bool (*propagate)(const struct charset_info_st *cs, const unsigned char *str, size_t len);
} MY_COLLATION_HANDLER;

/* See strings/charset_info_st.txt about information on this structure  */
typedef struct my_charset_handler_st
{
  bool (*init)(struct charset_info_st *, unsigned char *(*alloc)(size_t));
  /* Multibyte routines */
  uint32_t    (*ismbchar)(const struct charset_info_st * const, const char *, const char *);
  uint32_t    (*mbcharlen)(const struct charset_info_st * const, uint32_t c);
  size_t  (*numchars)(const struct charset_info_st * const, const char *b, const char *e);
  size_t  (*charpos)(const struct charset_info_st * const, const char *b, const char *e,
                     size_t pos);
  size_t  (*well_formed_len)(const struct charset_info_st * const,
                             const char *b,const char *e,
                             size_t nchars, int *error);
  size_t  (*lengthsp)(const struct charset_info_st * const, const char *ptr, size_t length);
  size_t  (*numcells)(const struct charset_info_st * const, const char *b, const char *e);

  /* Unicode conversion */
  my_charset_conv_mb_wc mb_wc;
  my_charset_conv_wc_mb wc_mb;

  /* CTYPE scanner */
  int (*ctype)(const struct charset_info_st *cs, int *ctype,
               const unsigned char *s, const unsigned char *e);

  /* Functions for case and sort conversion */
  size_t  (*caseup_str)(const struct charset_info_st * const, char *);
  size_t  (*casedn_str)(const struct charset_info_st * const, char *);

  my_charset_conv_case caseup;
  my_charset_conv_case casedn;

  /* Charset dependant snprintf() */
  size_t (*snprintf)(const struct charset_info_st * const, char *to, size_t n,
                     const char *fmt,
                     ...)
#if defined(__GNUC__) && !defined(__INTEL_COMPILER)
                         __attribute__((format(printf, 4, 5)))
#endif
                         ;
  size_t (*long10_to_str)(const struct charset_info_st * const, char *to, size_t n,
                          int radix, long int val);
  size_t (*int64_t10_to_str)(const struct charset_info_st * const, char *to, size_t n,
                              int radix, int64_t val);

  void (*fill)(const struct charset_info_st * const, char *to, size_t len, int fill);

  /* String-to-number conversion routines */
  long        (*strntol)(const struct charset_info_st * const, const char *s, size_t l,
			 int base, char **e, int *err);
  unsigned long      (*strntoul)(const struct charset_info_st * const, const char *s, size_t l,
			 int base, char **e, int *err);
  int64_t   (*strntoll)(const struct charset_info_st * const, const char *s, size_t l,
			 int base, char **e, int *err);
  uint64_t (*strntoull)(const struct charset_info_st * const, const char *s, size_t l,
			 int base, char **e, int *err);
  double      (*strntod)(const struct charset_info_st * const, char *s, size_t l, char **e,
			 int *err);
  int64_t    (*strtoll10)(const struct charset_info_st *cs,
                           const char *nptr, char **endptr, int *error);
  uint64_t   (*strntoull10rnd)(const struct charset_info_st *cs,
                                const char *str, size_t length,
                                int unsigned_fl,
                                char **endptr, int *error);
  size_t        (*scan)(const struct charset_info_st * const, const char *b, const char *e,
                        int sq);
} MY_CHARSET_HANDLER;


/* See strings/charset_info_st.txt about information on this structure  */
struct charset_info_st
{
  uint32_t      number;
  uint32_t      primary_number;
  uint32_t      binary_number;
  uint32_t      state;
  const char *csname;
  const char *name;
  const char *comment;
  const char *tailoring;
  unsigned char    *ctype;
  unsigned char    *to_lower;
  unsigned char    *to_upper;
  unsigned char    *sort_order;
  uint16_t   *contractions;
  uint16_t   **sort_order_big;
  uint16_t      *tab_to_uni;
  MY_UNI_IDX  *tab_from_uni;
  MY_UNICASE_INFO **caseinfo;
  unsigned char     *state_map;
  unsigned char     *ident_map;
  uint32_t      strxfrm_multiply;
  unsigned char     caseup_multiply;
  unsigned char     casedn_multiply;
  uint32_t      mbminlen;
  uint32_t      mbmaxlen;
  uint16_t    min_sort_char;
  uint16_t    max_sort_char; /* For LIKE optimization */
  unsigned char     pad_char;
  bool   escape_with_backslash_is_dangerous;
  unsigned char     levels_for_compare;
  unsigned char     levels_for_order;

  MY_CHARSET_HANDLER *cset;
  MY_COLLATION_HANDLER *coll;

};

extern DRIZZLED_API charset_info_st *all_charsets[256];
extern charset_info_st compiled_charsets[];
extern uint32_t get_charset_number(const char *cs_name, uint32_t cs_flags);
extern uint32_t get_collation_number(const char *name);
extern const char *get_charset_name(uint32_t cs_number);
extern bool resolve_charset(const char *cs_name,
			    const charset_info_st *default_cs,
			    const charset_info_st **cs);
extern bool resolve_collation(const char *cl_name,
			     const charset_info_st *default_cl,
			     const charset_info_st **cl);
extern void free_charsets(void);
extern char *get_charsets_dir(char *buf);
extern bool my_charset_same(const charset_info_st *cs1, const charset_info_st *cs2);
extern bool init_compiled_charsets(myf flags);
extern void add_compiled_collation(charset_info_st *cs);
extern size_t escape_string_for_drizzle(const charset_info_st *charset_info,
					char *to, size_t to_length,
					const char *from, size_t length);
extern size_t escape_quotes_for_drizzle(const charset_info_st *charset_info,
					char *to, size_t to_length,
					const char *from, size_t length);
extern DRIZZLED_API const charset_info_st *default_charset_info;
extern DRIZZLED_API const charset_info_st *system_charset_info;
extern const charset_info_st *files_charset_info;
extern const charset_info_st *table_alias_charset;
extern MY_UNICASE_INFO *my_unicase_default[256];
extern MY_UNICASE_INFO *my_unicase_turkish[256];
extern MY_UNI_CTYPE my_uni_ctype[256];
extern DRIZZLED_API charset_info_st my_charset_bin;
extern DRIZZLED_API charset_info_st my_charset_utf8mb4_bin;
extern DRIZZLED_API charset_info_st my_charset_utf8mb4_general_ci;
extern DRIZZLED_API charset_info_st my_charset_utf8mb4_unicode_ci;
size_t my_strnxfrmlen_simple(const charset_info_st * const, size_t);
extern int  my_strnncollsp_simple(const charset_info_st * const, const unsigned char *, size_t,
                                  const unsigned char *, size_t,
                                  bool diff_if_only_endspace_difference);
extern size_t my_lengthsp_8bit(const charset_info_st * const cs, const char *ptr, size_t length);
extern uint32_t my_instr_simple(const charset_info_st * const,
                            const char *b, size_t b_length,
                            const char *s, size_t s_length,
                            my_match_t *match, uint32_t nmatch);
extern MY_CHARSET_HANDLER my_charset_8bit_handler;
extern MY_CHARSET_HANDLER my_charset_ucs2_handler;
extern int my_strcasecmp_mb(const charset_info_st * const  cs, const char *s, const char *t);
extern bool my_parse_charset_xml(const char *bug, size_t len,
				    int (*add)(charset_info_st *cs));

DRIZZLED_API const charset_info_st *get_charset(uint32_t cs_number);
DRIZZLED_API const charset_info_st *get_charset_by_name(const char *cs_name);
DRIZZLED_API const charset_info_st *get_charset_by_csname(const char *cs_name, uint32_t cs_flags);

/* Functions for 8bit */
int my_mb_ctype_8bit(const charset_info_st * const,int *, const unsigned char *,const unsigned char *);
int my_mb_ctype_mb(const charset_info_st * const,int *, const unsigned char *,const unsigned char *);

size_t my_scan_8bit(const charset_info_st * const cs, const char *b, const char *e, int sq);

size_t my_snprintf_8bit(const charset_info_st * const, char *to, size_t n,
                        const char *fmt, ...)
  __attribute__((format(printf, 4, 5)));

long       my_strntol_8bit(const charset_info_st * const, const char *s, size_t l, int base,
                           char **e, int *err);
unsigned long      my_strntoul_8bit(const charset_info_st * const, const char *s, size_t l, int base,
			    char **e, int *err);
int64_t   my_strntoll_8bit(const charset_info_st * const, const char *s, size_t l, int base,
			    char **e, int *err);
uint64_t my_strntoull_8bit(const charset_info_st * const, const char *s, size_t l, int base,
			    char **e, int *err);
double      my_strntod_8bit(const charset_info_st * const, char *s, size_t l,char **e,
			    int *err);
size_t my_long10_to_str_8bit(const charset_info_st * const, char *to, size_t l, int radix,
                             long int val);
size_t my_int64_t10_to_str_8bit(const charset_info_st * const, char *to, size_t l, int radix,
                                 int64_t val);
int64_t my_strtoll10_8bit(const charset_info_st * const cs,
                           const char *nptr, char **endptr, int *error);
int64_t my_strtoll10_ucs2(charset_info_st *cs,
                           const char *nptr, char **endptr, int *error);

uint64_t my_strntoull10rnd_8bit(const charset_info_st * const cs,
                                 const char *str, size_t length, int
                                 unsigned_fl, char **endptr, int *error);
uint64_t my_strntoull10rnd_ucs2(charset_info_st *cs,
                                 const char *str, size_t length,
                                 int unsigned_fl, char **endptr, int *error);

void my_fill_8bit(const charset_info_st * const cs, char* to, size_t l, int fill);

bool  my_like_range_simple(const charset_info_st * const cs,
			      const char *ptr, size_t ptr_length,
			      char escape, char w_one, char w_many,
			      size_t res_length,
			      char *min_str, char *max_str,
			      size_t *min_length, size_t *max_length);

bool  my_like_range_mb(const charset_info_st * const cs,
			  const char *ptr, size_t ptr_length,
			  char escape, char w_one, char w_many,
			  size_t res_length,
			  char *min_str, char *max_str,
			  size_t *min_length, size_t *max_length);

bool  my_like_range_ucs2(const charset_info_st * const cs,
			    const char *ptr, size_t ptr_length,
			    char escape, char w_one, char w_many,
			    size_t res_length,
			    char *min_str, char *max_str,
			    size_t *min_length, size_t *max_length);

bool  my_like_range_utf16(const charset_info_st * const cs,
			     const char *ptr, size_t ptr_length,
			     char escape, char w_one, char w_many,
			     size_t res_length,
			     char *min_str, char *max_str,
			     size_t *min_length, size_t *max_length);

bool  my_like_range_utf32(const charset_info_st * const cs,
			     const char *ptr, size_t ptr_length,
			     char escape, char w_one, char w_many,
			     size_t res_length,
			     char *min_str, char *max_str,
			     size_t *min_length, size_t *max_length);


int my_wildcmp_8bit(const charset_info_st * const,
		    const char *str,const char *str_end,
		    const char *wildstr,const char *wildend,
		    int escape, int w_one, int w_many);

int my_wildcmp_bin(const charset_info_st * const,
		   const char *str,const char *str_end,
		   const char *wildstr,const char *wildend,
		   int escape, int w_one, int w_many);

size_t my_numchars_8bit(const charset_info_st * const, const char *b, const char *e);
size_t my_numcells_8bit(const charset_info_st * const, const char *b, const char *e);
size_t my_charpos_8bit(const charset_info_st * const, const char *b, const char *e, size_t pos);
size_t my_well_formed_len_8bit(const charset_info_st * const, const char *b, const char *e,
                             size_t pos, int *error);
typedef unsigned char *(*cs_alloc_func)(size_t);
bool my_coll_init_simple(charset_info_st *cs, cs_alloc_func alloc);
bool my_cset_init_8bit(charset_info_st *cs, cs_alloc_func alloc);
uint32_t my_mbcharlen_8bit(const charset_info_st * const, uint32_t c);

/* Functions for multibyte charsets */
int my_wildcmp_mb(const charset_info_st * const,
		  const char *str,const char *str_end,
		  const char *wildstr,const char *wildend,
		  int escape, int w_one, int w_many);
size_t my_numchars_mb(const charset_info_st * const, const char *b, const char *e);
size_t my_numcells_mb(const charset_info_st * const, const char *b, const char *e);
size_t my_charpos_mb(const charset_info_st * const, const char *b, const char *e, size_t pos);
size_t my_well_formed_len_mb(const charset_info_st * const, const char *b, const char *e,
                             size_t pos, int *error);
uint32_t my_instr_mb(const charset_info_st * const,
                 const char *b, size_t b_length,
                 const char *s, size_t s_length,
                 my_match_t *match, uint32_t nmatch);

int my_strnncoll_mb_bin(const charset_info_st * const  cs,
                        const unsigned char *s, size_t slen,
                        const unsigned char *t, size_t tlen,
                        bool t_is_prefix);

int my_strnncollsp_mb_bin(const charset_info_st * const cs,
                          const unsigned char *a, size_t a_length,
                          const unsigned char *b, size_t b_length,
                          bool diff_if_only_endspace_difference);

int my_wildcmp_mb_bin(const charset_info_st * const cs,
                      const char *str,const char *str_end,
                      const char *wildstr,const char *wildend,
                      int escape, int w_one, int w_many);

int my_strcasecmp_mb_bin(const charset_info_st * const, const char *s, const char *t);

void my_hash_sort_mb_bin(const charset_info_st * const,
                         const unsigned char *key, size_t len, uint32_t *nr1, uint32_t *nr2);

size_t my_strnxfrm_mb(const charset_info_st * const,
                      unsigned char *dst, size_t dstlen, uint32_t nweights,
                      const unsigned char *src, size_t srclen, uint32_t flags);

int my_wildcmp_unicode(const charset_info_st * const cs,
                       const char *str, const char *str_end,
                       const char *wildstr, const char *wildend,
                       int escape, int w_one, int w_many,
                       MY_UNICASE_INFO **weights);

bool my_propagate_simple(const charset_info_st * const cs, const unsigned char *str, size_t len);
bool my_propagate_complex(const charset_info_st * const cs, const unsigned char *str, size_t len);


uint32_t my_strxfrm_flag_normalize(uint32_t flags, uint32_t nlevels);
void my_strxfrm_desc_and_reverse(unsigned char *str, unsigned char *strend,
                                 uint32_t flags, uint32_t level);
size_t my_strxfrm_pad_desc_and_reverse(const charset_info_st * const cs,
                                       unsigned char *str, unsigned char *frmend, unsigned char *strend,
                                       uint32_t nweights, uint32_t flags, uint32_t level);

bool my_charset_is_ascii_compatible(const charset_info_st * const cs);

/*
  Compare 0-terminated UTF8 strings.

  SYNOPSIS
    my_strcasecmp_utf8mb3()
    cs                  character set handler
    s                   First 0-terminated string to compare
    t                   Second 0-terminated string to compare

  IMPLEMENTATION

  RETURN
    - negative number if s < t
    - positive number if s > t
    - 0 is the strings are equal
*/
int my_wc_mb_filename(const charset_info_st * const,
                  my_wc_t wc, unsigned char *s, unsigned char *e);

int my_mb_wc_filename(const charset_info_st * const,
                  my_wc_t *pwc, const unsigned char *s, const unsigned char *e);


unsigned int my_ismbchar_utf8mb4(const charset_info_st * const cs, const char *b, const char *e);
unsigned int my_mbcharlen_utf8mb4(const charset_info_st * const, uint32_t c);

size_t my_strnxfrmlen_utf8mb4(const charset_info_st * const, size_t len);
size_t
my_strnxfrm_utf8mb4(const charset_info_st * const cs,
                    unsigned char *dst, size_t dstlen, uint32_t nweights,
                    const unsigned char *src, size_t srclen, uint32_t flags);

int my_wildcmp_utf8mb4(const charset_info_st * const cs,
                   const char *str, const char *strend,
                   const char *wildstr, const char *wildend,
                   int escape, int w_one, int w_many);
int my_strnncollsp_utf8mb4(const charset_info_st * const cs,
                       const unsigned char *s, size_t slen,
                       const unsigned char *t, size_t tlen,
                       bool diff_if_only_endspace_difference);
int my_strcasecmp_utf8mb4(const charset_info_st * const cs,
                          const char *s, const char *t);

int my_strnncoll_utf8mb4(const charset_info_st * const cs,
                     const unsigned char *s, size_t slen,
                     const unsigned char *t, size_t tlen,
                     bool t_is_prefix);

int my_mb_wc_utf8mb4(const charset_info_st * const cs,
                 my_wc_t * pwc, const unsigned char *s, const unsigned char *e);

int my_wc_mb_utf8mb4(const charset_info_st * const cs,
                 my_wc_t wc, unsigned char *r, unsigned char *e);

size_t my_caseup_str_utf8mb4(const charset_info_st * const cs, char *src);
size_t my_casedn_str_utf8mb4(const charset_info_st * const cs, char *src);

size_t my_caseup_utf8mb4(const charset_info_st * const cs, char *src, size_t srclen,
                  char *dst, size_t dstlen);
size_t my_casedn_utf8mb4(const charset_info_st * const cs,
                  char *src, size_t srclen,
                  char *dst, size_t dstlen);


bool my_coll_init_uca(charset_info_st *cs, cs_alloc_func alloc);

int my_strnncoll_any_uca(const charset_info_st * const cs,
                         const unsigned char *s, size_t slen,
                         const unsigned char *t, size_t tlen,
                         bool t_is_prefix);

int my_strnncollsp_any_uca(const charset_info_st * const cs,
                           const unsigned char *s, size_t slen,
                           const unsigned char *t, size_t tlen,
                           bool diff_if_only_endspace_difference);

void my_hash_sort_any_uca(const charset_info_st * const cs,
                          const unsigned char *s, size_t slen,
                          uint32_t *n1, uint32_t *n2);

size_t my_strnxfrm_any_uca(const charset_info_st * const cs,
                           unsigned char *dst, size_t dstlen, uint32_t nweights,
                           const unsigned char *src, size_t srclen,
                           uint32_t flags);

int my_wildcmp_uca(const charset_info_st * const cs,
                   const char *str,const char *str_end,
                   const char *wildstr,const char *wildend,
                   int escape, int w_one, int w_many);

int my_strnncoll_8bit_bin(const charset_info_st * const,
                          const unsigned char *s, size_t slen,
                          const unsigned char *t, size_t tlen,
                          bool t_is_prefix);
int my_strnncollsp_8bit_bin(const charset_info_st * const,
                            const unsigned char *a, size_t a_length,
                            const unsigned char *b, size_t b_length,
                            bool diff_if_only_endspace_difference);
size_t my_case_str_bin(const charset_info_st * const, char *);
size_t my_case_bin(const charset_info_st * const, char *,
                   size_t srclen, char *, size_t);
int my_strcasecmp_bin(const charset_info_st * const,
                      const char *s, const char *t);
size_t my_strnxfrm_8bit_bin(const charset_info_st * const cs,
                     unsigned char * dst, size_t dstlen, uint32_t nweights,
                     const unsigned char *src, size_t srclen, uint32_t flags);
uint32_t my_instr_bin(const charset_info_st * const,
                      const char *b, size_t b_length,
                      const char *s, size_t s_length,
                      my_match_t *match, uint32_t nmatch);
size_t my_lengthsp_binary(const charset_info_st * const,
                          const char *, size_t length);
int my_mb_wc_bin(const charset_info_st * const,
                 my_wc_t *wc, const unsigned char *str,
                 const unsigned char *end);
int my_wc_mb_bin(const charset_info_st * const, my_wc_t wc,
                 unsigned char *str, unsigned char *end);
void my_hash_sort_8bit_bin(const charset_info_st * const,
                           const unsigned char *key, size_t len,
                           uint32_t *nr1, uint32_t *nr2);
bool my_coll_init_8bit_bin(charset_info_st *cs,
                           cs_alloc_func);
int my_strnncoll_binary(const charset_info_st * const,
                        const unsigned char *s, size_t slen,
                        const unsigned char *t, size_t tlen,
                        bool t_is_prefix);
int my_strnncollsp_binary(const charset_info_st * const cs,
                          const unsigned char *s, size_t slen,
                          const unsigned char *t, size_t tlen,
                          bool);

inline static bool my_isascii(char c)      
{
  return (!(c & ~0177));
}

inline static char my_toascii(char c)
{
  return (c & 0177);
}

inline static char my_tocntrl(char c) 
{
  return (c & 31);
}

inline static char my_toprint(char c)
{
  return (c | 64);
}

inline static char my_toupper(const charset_info_st *s, unsigned char c)
{
  return s->to_upper[c];
}

inline static char my_tolower(const charset_info_st *s, unsigned char c)
{
  return s->to_lower[c];
}

inline static bool my_isalpha(const charset_info_st *s, unsigned char c)
{
  return (s->ctype+1)[c] & (_MY_U | _MY_L);
}

inline static bool my_isupper(const charset_info_st *s, unsigned char c)
{
  return (s->ctype+1)[c] & _MY_U;
}

inline static bool my_islower(const charset_info_st *s, unsigned char c)
{
  return (s->ctype+1)[c] & _MY_L;
}

inline static bool my_isdigit(const charset_info_st *s, unsigned char c)
{
  return (s->ctype+1)[c] & _MY_NMR;
}

inline static bool my_isxdigit(const charset_info_st *s, unsigned char c)
{
  return (s->ctype+1)[c] & _MY_X;
}

inline static bool my_isalnum(const charset_info_st *s, unsigned char c) 
{
  return (s->ctype+1)[c] & (_MY_U | _MY_L | _MY_NMR);
}

inline static bool my_isspace(const charset_info_st *s, unsigned char c)
{
  return (s->ctype+1)[c] & _MY_SPC;
}

inline static bool my_ispunct(const charset_info_st *s, unsigned char c)  
{
  return (s->ctype+1)[c] & _MY_PNT;
}

inline static bool my_isprint(const charset_info_st *s, unsigned char c)  
{
  return (s->ctype+1)[c] & (_MY_PNT | _MY_U | _MY_L | _MY_NMR | _MY_B);
}

inline static bool my_isgraph(const charset_info_st *s, unsigned char c)
{
  return (s->ctype+1)[c] & (_MY_PNT | _MY_U | _MY_L | _MY_NMR);
}

inline static bool my_iscntrl(const charset_info_st *s, unsigned char c)  
{
  return (s->ctype+1)[c] & _MY_CTR;
}

/* Some macros that should be cleaned up a little */
inline static bool my_isvar(const charset_info_st *s, char c)
{
  return my_isalnum(s,c) || (c) == '_';
}

inline static bool my_isvar_start(const charset_info_st *s, char c)
{
  return my_isalpha(s,c) || (c) == '_';
}

inline static bool my_binary_compare(const charset_info_st *s)
{
  return s->state  & MY_CS_BINSORT;
}

inline static bool use_strnxfrm(const charset_info_st *s)
{
  return s->state & MY_CS_STRNXFRM;
}

inline static size_t my_strnxfrm(const charset_info_st *cs, 
                                 unsigned char *dst, 
                                 const size_t dstlen, 
                                 const unsigned char *src, 
                                 const uint32_t srclen)
{
  return (cs->coll->strnxfrm(cs, dst, dstlen, dstlen, src, srclen, MY_STRXFRM_PAD_WITH_SPACE));
}

inline static int my_strnncoll(const charset_info_st *cs, 
                               const unsigned char *s, 
                               const size_t slen, 
                               const unsigned char *t,
                               const size_t tlen) 
{
  return (cs->coll->strnncoll(cs, s, slen, t, tlen, 0));
}

inline static bool my_like_range(const charset_info_st *cs,
                                 const char *ptr, const size_t ptrlen,
                                 const char escape, 
                                 const char w_one,
                                 const char w_many, 
                                 const size_t reslen, 
                                 char *minstr, char *maxstr, 
                                 size_t *minlen, size_t *maxlen)
{
  return (cs->coll->like_range(cs, ptr, ptrlen, escape, w_one, w_many, reslen, 
                               minstr, maxstr, minlen, maxlen));
}

inline static int my_wildcmp(const charset_info_st *cs,
                             const char *str, const char *strend,
                             const char *w_str, const char *w_strend,
                             const int escape,
                             const int w_one, const int w_many) 
{
  return (cs->coll->wildcmp(cs, str, strend, w_str, w_strend, escape, w_one, w_many));
}

inline static int my_strcasecmp(const charset_info_st *cs, const char *s, const char *t)
{
  return (cs->coll->strcasecmp(cs, s, t));
}

template <typename CHAR_T>
inline static size_t my_charpos(const charset_info_st *cs, 
                                const CHAR_T *b, const CHAR_T* e, size_t num)
{
  return cs->cset->charpos(cs, reinterpret_cast<const char*>(b), reinterpret_cast<const char*>(e), num);
}

inline static bool use_mb(const charset_info_st *cs)
{
  return cs->cset->ismbchar != NULL;
}

inline static unsigned int  my_ismbchar(const charset_info_st *cs, const char *a, const char *b)
{
  return cs->cset->ismbchar(cs, a, b);
}

inline static unsigned int my_mbcharlen(const charset_info_st *cs, uint32_t c)
{
  return cs->cset->mbcharlen(cs, c);
}


inline static size_t my_caseup_str(const charset_info_st *cs, char *src)
{
  return cs->cset->caseup_str(cs, src);
}

inline static size_t my_casedn_str(const charset_info_st *cs, char *src)
{
  return cs->cset->casedn_str(cs, src);
}

inline static long my_strntol(const charset_info_st *cs, 
                              const char* s, const size_t l, const int base, char **e, int *err)
{
  return (cs->cset->strntol(cs, s, l, base, e, err));
}

inline static unsigned long my_strntoul(const charset_info_st *cs, 
                                        const char* s, const size_t l, const int base, 
                                        char **e, int *err)
{
  return (cs->cset->strntoul(cs, s, l, base, e, err));
}

inline static int64_t my_strntoll(const charset_info_st *cs, 
                                 const char* s, const size_t l, const int base, char **e, int *err)
{
  return (cs->cset->strntoll(cs, s, l, base, e, err));
}

inline static int64_t my_strntoull(const charset_info_st *cs, 
                                   const char* s, const size_t l, const int base, 
                                   char **e, int *err)
{
  return (cs->cset->strntoull(cs, s, l, base, e, err));
}


inline static double my_strntod(const charset_info_st *cs, 
                                char* s, const size_t l, char **e, int *err)
{
  return (cs->cset->strntod(cs, s, l, e, err));
}

int make_escape_code(const charset_info_st * const cs, const char *escape);

} /* namespace drizzled */

