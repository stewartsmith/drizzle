/* Copyright (C) 2000 MySQL AB

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

/* Resolve numeric stack dump produced by drizzled 3.23.30 and later
   versions into symbolic names. By Sasha Pachev <sasha@mysql.com>
 */

#include "config.h"
#include <iostream>
#include <cstdio>
#include <cerrno>

#include "drizzled/charset_info.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/internal/m_string.h"
#include "drizzled/option.h"
#include <boost/program_options.hpp>

using namespace std;
namespace po= boost::program_options;
using namespace drizzled;

#define INIT_SYM_TABLE  4096
#define INC_SYM_TABLE  4096
#define MAX_SYM_SIZE   128
#define DUMP_VERSION "1.4"
#define HEX_INVALID  (unsigned char)255

typedef struct sym_entry
{
  char symbol[MAX_SYM_SIZE];
  unsigned char* addr;
} SYM_ENTRY;


std::string dump_fname, sym_fname;
static DYNAMIC_ARRAY sym_table; /* how do you like this , static DYNAMIC ? */
static FILE* fp_dump, *fp_sym = 0, *fp_out;

static void verify_sort(void);

static void die(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s: ", internal::my_progname);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

static void open_files(void)
{
  fp_out = stdout;
  fp_dump = stdin;

  if (! dump_fname.empty() && !(fp_dump= fopen(dump_fname.c_str(), "r")))
      die("Could not open %s", dump_fname.c_str());
  /* if name not given, assume stdin*/

  if (sym_fname.empty())
    die("Please run nm --numeric-sort on drizzled binary that produced stack "
        "trace dump and specify the path to it with -s or --symbols-file");
  if (!(fp_sym= fopen(sym_fname.c_str(), "r")))
    die("Could not open %s", sym_fname.c_str());

}

static unsigned char hex_val(char c)
{
  unsigned char l;
  if (my_isdigit(&my_charset_utf8_general_ci,c))
    return c - '0';
  l = my_tolower(&my_charset_utf8_general_ci,c);
  if (l < 'a' || l > 'f')
    return HEX_INVALID;
  return (unsigned char)10 + ((unsigned char)c - (unsigned char)'a');
}

static unsigned long read_addr(char** buf)
{
  unsigned char c;
  char* p = *buf;
  unsigned long addr = 0;

  while((c = hex_val(*p++)) != HEX_INVALID)
      addr = (addr << 4) + c;

  *buf = p;
  return addr;
}

static int init_sym_entry(SYM_ENTRY* se, char* buf)
{
  char* p, *p_end;
  se->addr = (unsigned char*)read_addr(&buf);

  if (!se->addr)
    return -1;
  while (my_isspace(&my_charset_utf8_general_ci,*buf++))
    /* empty */;

  while (my_isspace(&my_charset_utf8_general_ci,*buf++))
    /* empty - skip more space */;
  --buf;
  /* now we are on the symbol */
  for (p = se->symbol, p_end = se->symbol + sizeof(se->symbol) - 1;
       *buf != '\n' && *buf && p < p_end; ++buf,++p)
    *p = *buf;
  *p = 0;
  if (!strcmp(se->symbol, "gcc2_compiled."))
    return -1;
  return 0;
}

static void init_sym_table(void)
{
  char buf[512];
  if (my_init_dynamic_array(&sym_table, sizeof(SYM_ENTRY), INIT_SYM_TABLE,
			    INC_SYM_TABLE))
    die("Failed in my_init_dynamic_array() -- looks like out of memory problem");

  while (fgets(buf, sizeof(buf), fp_sym))
  {
    SYM_ENTRY se;
    if (init_sym_entry(&se, buf))
      continue;
    if (insert_dynamic(&sym_table, (unsigned char*)&se))
      die("insert_dynamic() failed - looks like we are out of memory");
  }

  verify_sort();
}

static void clean_up(void)
{
  delete_dynamic(&sym_table);
}

static void verify_sort()
{
  uint32_t i;
  unsigned char* last = 0;

  for (i = 0; i < sym_table.elements; i++)
  {
    SYM_ENTRY se;
    get_dynamic(&sym_table, (unsigned char*)&se, i);
    if (se.addr < last)
      die("sym table does not appear to be sorted, did you forget "
          "--numeric-sort arg to nm? trouble addr = %p, last = %p",
          se.addr, last);
    last = se.addr;
  }
}


static SYM_ENTRY* resolve_addr(unsigned char* addr, SYM_ENTRY* se)
{
  uint32_t i;
  get_dynamic(&sym_table, (unsigned char*)se, 0);
  if (addr < se->addr)
    return 0;

  for (i = 1; i < sym_table.elements; i++)
  {
    get_dynamic(&sym_table, (unsigned char*)se, i);
    if (addr < se->addr)
    {
      get_dynamic(&sym_table, (unsigned char*)se, i - 1);
      return se;
    }
  }

  return se;
}


static void do_resolve(void)
{
  char buf[1024], *p;
  while (fgets(buf, sizeof(buf), fp_dump))
  {
    p = buf;
    /* skip space */
    while (my_isspace(&my_charset_utf8_general_ci,*p))
      ++p;

    if (*p++ == '0' && *p++ == 'x')
    {
      SYM_ENTRY se ;
      unsigned char* addr = (unsigned char*)read_addr(&p);
      if (resolve_addr(addr, &se))
	fprintf(fp_out, "%p %s + %d\n", addr, se.symbol,
		(int) (addr - se.addr));
      else
	fprintf(fp_out, "%p (?)\n", addr);

    }
    else
    {
      fputs(buf, fp_out);
      continue;
    }
  }
}


int main(int argc, char** argv)
{
try
{
  MY_INIT(argv[0]);

  po::options_description commandline_options("Options specific to the commandline");
  commandline_options.add_options()
  ("help,h", "Display this help and exit.")
  ("version,V", "Output version information and exit.") 
  ;

  po::options_description positional_options("Positional Options");
  positional_options.add_options()
  ("symbols-file,s", po::value<string>(),
  "Use specified symbols file.")
  ("numeric-dump-file,n", po::value<string>(),
  "Read the dump from specified file.")
  ;

  po::options_description long_options("Allowed Options");
  long_options.add(commandline_options).add(positional_options);

  po::positional_options_description p;
  p.add("symbols-file,s", 1);
  p.add("numeric-dump-file,n",1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(long_options).positional(p).run(), vm);
  po::notify(vm);

  if (vm.count("help"))
  {
    printf("%s  Ver %s Distrib %s, for %s-%s (%s)\n",internal::my_progname,DUMP_VERSION,
	 VERSION,HOST_VENDOR,HOST_OS,HOST_CPU);
    printf("MySQL AB, by Sasha Pachev\n");
    printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
    printf("Resolve numeric stack strace dump into symbols.\n\n");
    printf("Usage: %s [OPTIONS] symbols-file [numeric-dump-file]\n",
	  internal::my_progname);
    cout << long_options << endl;
    printf("\n"
         "The symbols-file should include the output from: \n"
         "  'nm --numeric-sort drizzled'.\n"
         "The numeric-dump-file should contain a numeric stack trace "
         "from drizzled.\n"
         "If the numeric-dump-file is not given, the stack trace is "
         "read from stdin.\n");
  }
    
  if (vm.count("version"))
  {
    printf("%s  Ver %s Distrib %s, for %s-%s (%s)\n",internal::my_progname,DUMP_VERSION,
	 VERSION,HOST_VENDOR,HOST_OS,HOST_CPU);
  }
    
  if (vm.count("symbols-file") && vm.count("numeric-file-dump"))
  {
    sym_fname= vm["symbols-file"].as<string>();
    dump_fname= vm["numeric-dump-file"].as<string>();
  }

  open_files();
  init_sym_table();
  do_resolve();
  clean_up();
}
  catch(exception &err)
  {
    cerr << err.what() << endl;
  }
 
   return 0;
}
