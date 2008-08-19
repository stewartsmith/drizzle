
#include "binlog_encoding.h"

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <getopt.h>

using std::ios;
using std::cout;
using std::cerr;
using std::flush;

void print_usage_and_exit(char *prog) {
  const char *name= strrchr(prog, '/');
  if (name)
    ++name;
  else
    name= "length";
  cerr << "Usage:\n"
       << "    " << name << " [ -vvvx ] -e <number> ...\n";
  cerr << "    " << name << " [ -vvvx ] -d <byte> ...\n";
  cerr << flush;
  exit(1);
}

void encode(int argc, char *argv[], int verbose_level, bool hex_output) {
  for (int i = 0 ; i < argc ; ++i) {
    size_t length = strtoul(argv[i], NULL, 0);

    if (length < 2)
      throw std::invalid_argument("Length has to be > 1");

    unsigned char buf[128];
    unsigned char *end= length_encode(length, buf);
    ios::fmtflags saved_flags= cout.flags();
    if (verbose_level > 0)
      cout << "Length " << length << ": ";
    if (hex_output)
      cout << std::hex << std::setw(2) << std::setfill('0');
    unsigned char *ptr= buf;
    while (true) {
      if (hex_output)
        cout << "0x";
      cout << (unsigned int) *ptr;
      if (++ptr == end)
        break;
      cout << " ";
    }
    cout << std::endl;
    cout.setf(saved_flags);
  }
}


void decode(int argc, char *argv[], int verbose_level, bool hex_output) {
  unsigned char buf[128];
  for (int i = 0 ; i < argc ; ++i)
    buf[i]= strtoul(argv[i], NULL, 0);

  size_t length;
  (void) length_decode(buf, &length);

  ios::fmtflags saved_flags= cout.flags();
  if (verbose_level > 0)
    cout << "Length ";
  if (hex_output)
    cout.setf(ios::hex, ios::basefield);
  cout << length << std::endl;
  cout.setf(saved_flags);
}


int main(int argc, char *argv[]) {
  enum { NO_ACTION, ENCODE_ACTION, DECODE_ACTION } action= NO_ACTION;

  static struct option long_options[] = {
    { "decode",  0 /* has_arg */, NULL, 'd' },
    { "encode",  0 /* has_arg */, NULL, 'e' },
    { "verbose", 0 /* has_arg */, NULL, 'v' },
    { "hex",     0 /* has_arg */, NULL, 'x' },
    { 0, 0, 0, 0 }
  };

  int verbose_level= 0;
  bool hex_output= false;
  int ch;

  while ((ch= getopt_long(argc, argv, "devx", long_options, NULL)) != -1) {
    switch (ch) {
    case 0:
    case '?':
      print_usage_and_exit(argv[0]);
      break;

    case 'd':
      action= DECODE_ACTION;
      break;

    case 'e':
      action= ENCODE_ACTION;
      break;

    case 'v':
      ++verbose_level;
      break;

    case 'x':
      hex_output= true;
      break;
    }
  }

  try {
    switch (action) {
    case ENCODE_ACTION:
      encode(argc - optind, argv + optind, verbose_level, hex_output);
      break;
    case DECODE_ACTION:
      decode(argc - optind, argv + optind, verbose_level, hex_output);
      break;
    default:
      print_usage_and_exit(argv[0]);
      break;
    }
  }
  catch (std::invalid_argument& ex) {
    cerr << ex.what() << "\n";
    print_usage_and_exit(argv[0]);
  }

  return EXIT_SUCCESS;
}
