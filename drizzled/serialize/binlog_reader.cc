#include <drizzled/global.h>

#include <drizzled/serialize/binary_log.h>

#include <iostream>
#include <fstream>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <getopt.h>
#include <fcntl.h>

#include <sys/stat.h>

using namespace google::protobuf;
using namespace google::protobuf::io;

void print_usage_and_exit(char *prog) {
  using std::cerr;
  const char *name= strrchr(prog, '/');

  if (name)
    ++name;
  else
    name= "binlog_reader";
  cerr << "Usage: " << name << " <options>\n"
       << "    --input name   Read queries from file <name> (default: 'log.bin')\n"
       << std::flush;
  exit(1);
}


void
print_event(BinaryLog::Event *event)
{
}


int
main(int argc, char *argv[])
{
  using std::ios;

  static struct option options[] = {
    { "input",     1 /* has_arg */, NULL, 0 },
    { 0, 0, 0, 0 }
  };

  const char *file_name= "log.bin";

  int ch, option_index;
  while ((ch= getopt_long(argc, argv, "", options, &option_index)) != -1) {
    if (ch == '?')
      print_usage_and_exit(argv[0]);

    switch (option_index) {
    case 0:                                     // --input
      file_name= optarg;
      break;

    }
  }

  if (optind > argc)
    print_usage_and_exit(argv[0]);

  filebuf fb;

  fb.open(file_name, std::ios::in);
  istream is(&fb);

  ZeroCopyInputStream* raw_input = new IstreamInputStream(&is);
  CodedInputStream *coded_input = new CodedInputStream(raw_input);

  BinaryLog::Event event;
  while (event.read(coded_input))
    event.print(std::cout);

  delete coded_input;
  delete raw_input;
  fb.close();
}
