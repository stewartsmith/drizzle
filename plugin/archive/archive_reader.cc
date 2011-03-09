/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2006 MySQL AB
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#include <config.h>
#include <iostream>
#include <string>
#include <fstream>
#include <drizzled/configmake.h>
using namespace std;
#include <boost/program_options.hpp>
#include <boost/scoped_ptr.hpp>
namespace po= boost::program_options;
#include "azio.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <memory>
#include <drizzled/charset_info.h>
#include <drizzled/internal/m_string.h>

#define SHOW_VERSION "0.1"

using namespace drizzled;

string opt_tmpdir;
uint64_t new_auto_increment_value;
bool opt_check, 
  opt_force,
  opt_backup, 
  opt_extract_table_message, 
  opt_silent,
  opt_quiet,
  opt_quick,
  opt_autoincrement;

int main(int argc, char *argv[])
{
try
{
  po::options_description commandline_options("Options used only in command line");
  commandline_options.add_options()
  ("force,f",po::value<bool>(&opt_force)->default_value(false)->zero_tokens(),
  "Restart with -r if there are any errors in the table")
  ("help,?","Display this help and exit")
  ("version,V","Print version and exit")
  ("no-defaults", po::value<bool>()->default_value(false)->zero_tokens(),
  "Configuration file defaults are not used if no-defaults is set")
  ;

  po::options_description archive_reader_options("Options specific to the archive reader");
  archive_reader_options.add_options()
  ("tmpdir,t",po::value<string>(&opt_tmpdir)->default_value(""),
  "Path for temporary files.") 
  ("set-auto-increment,A",po::value<uint64_t>(&new_auto_increment_value)->default_value(0),
  "Force auto_increment to start at this or higher value. If no value is given, then sets the next auto_increment value to the highest used value for the auto key + 1.")
  ("silent,s",po::value<bool>(&opt_silent)->default_value(false)->zero_tokens(),
  "Only print errors. One can use two -s to make archive_reader very silent.")
  ("quick,q",po::value<bool>(&opt_quick)->default_value(false)->zero_tokens(),
  "Faster repair but not modifying the data")
  ("repair,r",po::value<bool>(&opt_quick)->default_value(false)->zero_tokens(),
  "Repair a damaged Archive version 3 or above file.")
  ("back-up,b",po::value<bool>(&opt_backup)->default_value(false)->zero_tokens(),
  "Make a backup of an archive table.")
  ("check,c",po::value<bool>(&opt_check)->default_value(false)->zero_tokens(),
  "Check table for errors")
  ("extract-table-message,e",po::value<bool>(&opt_extract_table_message)->default_value(false)->zero_tokens(),
  "Extract the table protobuf message.")
  ;

  unsigned int ret;
  boost::scoped_ptr<azio_stream> reader_handle_ap(new azio_stream);
  azio_stream &reader_handle= *reader_handle_ap.get();

  std::string system_config_dir_archive_reader(SYSCONFDIR); 
  system_config_dir_archive_reader.append("/drizzle/archive_reader.cnf");

  std::string user_config_dir((getenv("XDG_CONFIG_HOME")? getenv("XDG_CONFIG_HOME"):"~/.config"));

  if (user_config_dir.compare(0, 2, "~/") == 0)
  {
    char *homedir;
    homedir= getenv("HOME");
    if (homedir != NULL)
      user_config_dir.replace(0, 1, homedir);
  }
  
  po::options_description long_options("Allowed Options");
  long_options.add(commandline_options).add(archive_reader_options);

  po::variables_map vm;
  po::store(po::parse_command_line(argc,argv,long_options),vm);

  if (! vm["no-defaults"].as<bool>())
  {
    std::string user_config_dir_archive_reader(user_config_dir);
    user_config_dir_archive_reader.append("/drizzle/archive_reader.cnf");
  
    ifstream user_archive_reader_ifs(user_config_dir_archive_reader.c_str());
    po::store(parse_config_file(user_archive_reader_ifs, archive_reader_options), vm);

    ifstream system_archive_reader_ifs(system_config_dir_archive_reader.c_str());
    store(parse_config_file(system_archive_reader_ifs, archive_reader_options), vm);

  }
  po::notify(vm);

  if (vm.count("force") || vm.count("quiet") || vm.count("tmpdir"))
    cout << "Not implemented yet";

  if (vm.count("version"))
  {
    printf("%s  Ver %s, for %s-%s (%s)\n", internal::my_progname, SHOW_VERSION,
        HOST_VENDOR, HOST_OS, HOST_CPU);
    exit(0);
  }
  
  if (vm.count("set-auto-increment"))
  {
    opt_autoincrement= true;
  }

  if (vm.count("help") || argc == 0)
  {
    printf("%s  Ver %s, for %s-%s (%s)\n", internal::my_progname, SHOW_VERSION,
        HOST_VENDOR, HOST_OS, HOST_CPU);
    puts("This software comes with ABSOLUTELY NO WARRANTY. This is free "
         "software,\n"
         "and you are welcome to modify and redistribute it under the GPL "
         "license\n");
    puts("Read and modify Archive files directly\n");
    printf("Usage: %s [OPTIONS] file_to_be_looked_at [file_for_backup]\n", internal::my_progname);
    cout << long_options << endl;    
    exit(0); 
  }
  
  if (argc < 1)
  {
    printf("No file specified. \n");
    return -1;
  }

  if (!(ret= azopen(&reader_handle, argv[0], O_RDONLY, AZ_METHOD_BLOCK)))
  {
    printf("Could not open Archive file\n");
    return -1;
  }

  if (opt_autoincrement)
  {
    boost::scoped_ptr<azio_stream> writer_handle_ap(new azio_stream);
    azio_stream &writer_handle= *writer_handle_ap.get();

    if (new_auto_increment_value)
    {
      if (reader_handle.auto_increment >= new_auto_increment_value)
      {
        printf("Value is lower then current value\n");
        goto end;
      }
    }
    else
    {
      new_auto_increment_value= reader_handle.auto_increment + 1;
    }

    if (!(ret= azopen(&writer_handle, argv[0], O_CREAT|O_RDWR,
                      AZ_METHOD_BLOCK)))
    {
      printf("Could not open file for update: %s\n", argv[0]);
      goto end;
    }

    writer_handle.auto_increment= new_auto_increment_value;

    azclose(&writer_handle);
    azflush(&reader_handle, Z_SYNC_FLUSH);
  }

  printf("Version %u\n", reader_handle.version);
  if (reader_handle.version > 2)
  {
    printf("\tMinor version %u\n", reader_handle.minor_version);
    printf("\tStart position %"PRIu64"\n", (uint64_t)reader_handle.start);
    printf("\tBlock size %u\n", reader_handle.block_size);
    printf("\tRows %"PRIu64"\n", reader_handle.rows);
    printf("\tAutoincrement %"PRIu64"\n", reader_handle.auto_increment);
    printf("\tCheck Point %"PRIu64"\n", reader_handle.check_point);
    printf("\tForced Flushes %"PRIu64"\n", reader_handle.forced_flushes);
    printf("\tLongest Row %u\n", reader_handle.longest_row);
    printf("\tShortest Row %u\n", reader_handle.shortest_row);
    printf("\tState %s\n", ( reader_handle.dirty ? "dirty" : "clean"));
    printf("\tTable protobuf message stored at %u\n",
           reader_handle.frm_start_pos);
    printf("\tComment stored at %u\n", reader_handle.comment_start_pos);
    printf("\tData starts at %u\n", (unsigned int)reader_handle.start);
    if (reader_handle.frm_start_pos)
      printf("\tTable proto message length %u\n", reader_handle.frm_length);
    if (reader_handle.comment_start_pos)
    {
      char *comment =
        (char *) malloc(sizeof(char) * reader_handle.comment_length);
      azread_comment(&reader_handle, comment);
      printf("\tComment length %u\n\t\t%.*s\n", reader_handle.comment_length,
             reader_handle.comment_length, comment);
      free(comment);
    }
  }
  else
  {
    goto end;
  }

  printf("\n");

  if (opt_check)
  {
    int error;
    unsigned int row_read;
    uint64_t row_count= 0;

    while ((row_read= azread_row(&reader_handle, &error)))
    {
      if (error == Z_STREAM_ERROR)
      {
        printf("Table is damaged\n");
        goto end;
      }

      row_count++;

      if (row_read > reader_handle.longest_row)
      {
        printf("Table is damaged, row %"PRIu64" is invalid\n", row_count);
        goto end;
      }
    }

    printf("Found %"PRIu64" rows\n", row_count);
  }

  if (opt_backup)
  {
    int error;
    unsigned int row_read;
    uint64_t row_count= 0;
    char *buffer;

    boost::scoped_ptr<azio_stream> writer_handle_ap(new azio_stream);
    azio_stream &writer_handle= *writer_handle_ap.get();

    buffer= (char *)malloc(reader_handle.longest_row);
    if (buffer == NULL)
    {
      printf("Could not allocate memory for row %"PRIu64"\n", row_count);
      goto end;
    }


    if (!(ret= azopen(&writer_handle, argv[1], O_CREAT|O_RDWR,
                      AZ_METHOD_BLOCK)))
    {
      printf("Could not open file for backup: %s\n", argv[1]);
      goto end;
    }

    writer_handle.auto_increment= reader_handle.auto_increment;
    if (reader_handle.frm_length)
    {
      char *ptr;
      ptr= (char *)malloc(sizeof(char) * reader_handle.frm_length);
      if (ptr == NULL)
      {
        printf("Could not allocate enough memory\n");
        goto end;
      }
      azread_frm(&reader_handle, ptr);
      azwrite_frm(&writer_handle, ptr, reader_handle.frm_length);
      free(ptr);
    }

    if (reader_handle.comment_length)
    {
      char *ptr;
      ptr= (char *)malloc(sizeof(char) * reader_handle.comment_length);
      azread_comment(&reader_handle, ptr);
      azwrite_comment(&writer_handle, ptr, reader_handle.comment_length);
      free(ptr);
    }

    while ((row_read= azread_row(&reader_handle, &error)))
    {
      if (error == Z_STREAM_ERROR || error)
      {
        printf("Table is damaged\n");
        goto end;
      }

      /* If we read nothing we are at the end of the file */
      if (row_read == 0)
        break;

      row_count++;

      azwrite_row(&writer_handle, reader_handle.row_ptr, row_read);

      if (reader_handle.rows == writer_handle.rows)
        break;
    }

    free(buffer);

    azclose(&writer_handle);
  }

  if (opt_extract_table_message)
  {
    int frm_file;
    char *ptr;
    frm_file= internal::my_open(argv[1], O_CREAT|O_RDWR, MYF(0));
    ptr= (char *)malloc(sizeof(char) * reader_handle.frm_length);
    if (ptr == NULL)
    {
      printf("Could not allocate enough memory\n");
      goto end;
    }
    azread_frm(&reader_handle, ptr);
    internal::my_write(frm_file, (unsigned char*) ptr, reader_handle.frm_length, MYF(0));
    internal::my_close(frm_file, MYF(0));
    free(ptr);
  }

end:
  printf("\n");
  azclose(&reader_handle);

  internal::my_end();

}
  catch(exception &e)
  {
    cerr<<"Error"<<e.what()<<endl;
  }
  return 0;
}

