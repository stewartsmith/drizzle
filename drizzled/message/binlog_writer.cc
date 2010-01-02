/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#include "config.h"

#include <drizzled/message/binlog_encoding.h>
#include <drizzled/message/binary_log.h>

#include "drizzled/message/ioutil.h"

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>

#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace drizzled;
using namespace std;
using namespace google;

typedef std::map<std::string,std::string> Assign;

static void print_usage_and_exit(char *prog) {
  using std::cerr;
  const char *name= strrchr(prog, '/');

  if (name)
    ++name;
  else
    name= "binlog_writer";
  cerr << "Usage: " << name << " <options> <query>\n"
       << "    --output name    Append query to file <name> (default: 'log.bin')\n"
       << "    --set var=val    Set value of user variable for query\n"
       << "    --trans-id <id>  Set transaction id to <id>\n"
       << flush;
  exit(1);
}


static void
write_query(protobuf::io::CodedOutputStream* out,
            unsigned long trans_id,
            const string& query,
            const Assign& assign)
{
  BinaryLog::Query *message = new BinaryLog::Query;

  {
    BinaryLog::Header *header= message->mutable_header();
    header->set_seqno(static_cast<google::protobuf::uint32>(time(NULL)));
    header->set_server_id(1);
    header->set_trans_id(static_cast<google::protobuf::uint32>(trans_id));
  }

  message->set_query(query);
  for (Assign::const_iterator ii= assign.begin() ;
       ii != assign.end() ;
       ++ii )
  {
    BinaryLog::Query::Variable *var= message->add_variable();
    var->set_name(ii->first);
    var->set_val(ii->second);
  }

  BinaryLog::Event event(BinaryLog::Event::QUERY, message);
  event.write(out);
}


int main(int argc, char *argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  static struct option options[] = {
    { "set",       1 /* has_arg */, NULL, 0 },
    { "trans-id",  1 /* has_arg */, NULL, 0 },
    { "output",    1 /* has_arg */, NULL, 0 },
    { 0, 0, 0, 0 }
  };

  Assign assign;
  unsigned long trans_id= 0;
  const char* file_name= "log.bin";

  int ch, option_index;
  while ((ch= getopt_long(argc, argv, "", options, &option_index)) != -1) {
    if (ch == '?')
      print_usage_and_exit(argv[0]);

    switch (option_index) {
    case 0:                                     // --set
    {
      // Split the supplied string at the first '='
      char *end= optarg + strlen(optarg);
      char *pos= strchr(optarg, '=');
      if (!pos)
        pos= end;
      const string key(optarg, pos);
      const string value(pos == end ? end : pos+1, end);
      assign[key]= value;
    }

    case 1:                                     // --trans-id
      trans_id= strtoul(optarg, NULL, 0);
      break;

    case 2:                                     // --output
      file_name= optarg;
      break;
    }
  }

  if (optind >= argc)
    print_usage_and_exit(argv[0]);

  filebuf fb;

  fb.open(file_name, ios::app | ios::out);

  ostream os(&fb);

  protobuf::io::ZeroCopyOutputStream* raw_output=
    new protobuf::io::OstreamOutputStream(&os);
  protobuf::io::CodedOutputStream* coded_output=
    new protobuf::io::CodedOutputStream(raw_output);

  stringstream sout;
  sout << message::ioutil::join(" ", &argv[optind], &argv[argc]);

  write_query(coded_output, trans_id, sout.str(), assign);

  delete coded_output;
  delete raw_output;
  fb.close();
  return 0;
}
