/* Copyright (C) 2000-2006 MySQL AB

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

/* maintaince of mysql databases */

#include "client_priv.h"
#include <signal.h>
#include <my_pthread.h>				/* because of signal()	*/
#include <sys/stat.h>

#define ADMIN_VERSION "8.42"
#define SHUTDOWN_DEF_TIMEOUT 3600		/* Wait for shutdown */

char *host= NULL;
static bool interrupted=0,opt_verbose=0;
static uint32_t tcp_port = 0, option_wait = 0, option_silent=0;
static uint32_t my_end_arg;
static uint32_t opt_connect_timeout, opt_shutdown_timeout;
static char *unix_port=0;
static myf error_flags; /* flags to pass to my_printf_error, like ME_BELL */

/*
  Forward declarations 
*/
static void usage(void);
static void print_version(void);
extern "C" sig_handler endprog(int signal_number);
extern "C" bool get_one_option(int optid, const struct my_option *opt,
                               char *argument);
static int execute_commands(MYSQL *mysql,int argc, char **argv);
static bool sql_connect(MYSQL *mysql, uint wait);
static bool get_pidfile(MYSQL *mysql, char *pidfile);
static bool wait_pidfile(char *pidfile, time_t last_modified,
                         struct stat *pidfile_status);

/*
  The order of commands must be the same as command_names,
  except ADMIN_ERROR
*/
enum commands {
  ADMIN_ERROR,
  ADMIN_SHUTDOWN,
  ADMIN_PING
};

static const char *command_names[]= {
  "shutdown",
  "ping",
  NullS
};

static TYPELIB command_typelib=
{ array_elements(command_names)-1,"commands", command_names, NULL };

static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", (char**) &host, (char**) &host, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   (char**) &tcp_port,
   (char**) &tcp_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_MYSQL_PROTOCOL, "The protocol of connection (tcp,socket,pipe,memory).",
    0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"set-variable", 'O',
   "Change the value of a variable. Please note that this option is deprecated; you can set variables directly with --variable-name=value.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Silently exit if one can't connect to server.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection.",
   (char**) &unix_port, (char**) &unix_port, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"verbose", 'v', "Write more information.", (char**) &opt_verbose,
   (char**) &opt_verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"wait", 'w', "Wait and retry if connection is down.", 0, 0, 0, GET_UINT,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"connect_timeout", OPT_CONNECT_TIMEOUT, "", (char**) &opt_connect_timeout,
   (char**) &opt_connect_timeout, 0, GET_ULONG, REQUIRED_ARG, 3600*12, 0,
   3600*12, 0, 1, 0},
  {"shutdown_timeout", OPT_SHUTDOWN_TIMEOUT, "", (char**) &opt_shutdown_timeout,
   (char**) &opt_shutdown_timeout, 0, GET_ULONG, REQUIRED_ARG,
   SHUTDOWN_DEF_TIMEOUT, 0, 3600*12, 0, 1, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static const char *load_default_groups[]= { "mysqladmin","client",0 };

bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
               char *argument)
{
  int error = 0;

  switch(optid) {
  case 's':
    option_silent++;
    break;
  case 'V':
    print_version();
    exit(0);
    break;
  case 'w':
    if (argument)
    {
      if ((option_wait=atoi(argument)) <= 0)
        option_wait=1;
    }
    else
      option_wait= ~(uint)0;
    break;
  case '?':
  case 'I':					/* Info */
    error++;
    break;
  }

  if (error)
  {
    usage();
    exit(1);
  }
  return 0;
}

int main(int argc,char *argv[])
{
  int error= 0, ho_error;
  MYSQL mysql;
  char **commands, **save_argv;

  MY_INIT(argv[0]);
  mysql_init(&mysql);
  load_defaults("my",load_default_groups,&argc,&argv);
  save_argv = argv;				/* Save for free_defaults */
  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
  {
    free_defaults(save_argv);
    exit(ho_error);
  }

  if (argc == 0)
  {
    usage();
    exit(1);
  }

  commands = argv;

  VOID(signal(SIGINT,endprog));			/* Here if abort */
  VOID(signal(SIGTERM,endprog));		/* Here if abort */

  if (opt_connect_timeout)
  {
    uint tmp=opt_connect_timeout;
    mysql_options(&mysql,MYSQL_OPT_CONNECT_TIMEOUT, (char*) &tmp);
  }


  error_flags= (myf)0;

  if (sql_connect(&mysql, option_wait))
  {
    unsigned int err= mysql_errno(&mysql);
    if (err >= CR_MIN_ERROR && err <= CR_MAX_ERROR)
      error= 1;
    else
    {
      /* Return 0 if all commands are PING */
      for (; argc > 0; argv++, argc--)
      {
        if (find_type(argv[0], &command_typelib, 2) != ADMIN_PING)
        {
          error= 1;
          break;
        }
      }
    }
  }
  else
  {
    error=execute_commands(&mysql,argc,commands);
    mysql_close(&mysql);
  }
  free_defaults(save_argv);
  my_end(my_end_arg);
  exit(error ? 1 : 0);
}

sig_handler endprog(int signal_number __attribute__((unused)))
{
  interrupted=1;
}

static bool sql_connect(MYSQL *mysql, uint wait)
{
  bool info=0;

  for (;;)
  {
    if (mysql_real_connect(mysql,host,NULL,NULL,NullS,tcp_port,
			   unix_port, 0))
    {
      mysql->reconnect= 1;
      if (info)
      {
        fputs("\n",stderr);
        (void) fflush(stderr);
      }
      return 0;
    }

    if (!wait)
    {
      if (!option_silent)
      {
        if (!host)
          host= (char*) LOCAL_HOST;

        my_printf_error(0,"connect to server at '%s' failed\nerror: '%s'",
        error_flags, host, mysql_error(mysql));
        if (mysql_errno(mysql) == CR_CONNECTION_ERROR)
        {
          fprintf(stderr,
                  "Check that mysqld is running and that the socket: '%s' exists!\n",
                  unix_port ? unix_port : mysql_unix_port);
        }
        else if (mysql_errno(mysql) == CR_CONN_HOST_ERROR ||
          mysql_errno(mysql) == CR_UNKNOWN_HOST)
        {
          fprintf(stderr,"Check that mysqld is running on %s",host);
          fprintf(stderr," and that the port is %d.\n",
          tcp_port ? tcp_port: mysql_port);
          fprintf(stderr,"You can check this by doing 'telnet %s %d'\n",
                  host, tcp_port ? tcp_port: mysql_port);
        }
      }
      return 1;
    }
    if (wait != (uint) ~0)
      wait--;				/* One less retry */
    if ((mysql_errno(mysql) != CR_CONN_HOST_ERROR) &&
        (mysql_errno(mysql) != CR_CONNECTION_ERROR))
    {
      fprintf(stderr,"Got error: %s\n", mysql_error(mysql));
    }
    else if (!option_silent)
    {
      if (!info)
      {
        info=1;
        fputs("Waiting for MySQL server to answer",stderr);
        (void) fflush(stderr);
      }
      else
      {
        putc('.',stderr);
        (void) fflush(stderr);
      }
    }
    sleep(5);
  }
}

/*
  Execute a command.
  Return 0 on ok
	 -1 on retryable error
	 1 on fatal error
*/
static int execute_commands(MYSQL *mysql,int argc, char **argv)
{

  /*
    MySQL documentation relies on the fact that mysqladmin will
    execute commands in the order specified.
    If this behaviour is ever changed, Docs should be notified.
  */
  for (; argc > 0 ; argv++,argc--)
  {
    switch (find_type(argv[0],&command_typelib,2)) {
    case ADMIN_SHUTDOWN:
    {
      char pidfile[FN_REFLEN];
      bool got_pidfile= 0;
      time_t last_modified= 0;
      struct stat pidfile_status;

      /*
        Only wait for pidfile on local connections if pidfile doesn't 
        exist, continue without pid file checking
      */
      if (mysql->unix_socket && (got_pidfile= !get_pidfile(mysql, pidfile)) &&
	        !stat(pidfile, &pidfile_status))
	      last_modified= pidfile_status.st_mtime;

      if (mysql_shutdown(mysql, SHUTDOWN_DEFAULT))
      {
        my_printf_error(0, "shutdown failed; error: '%s'", error_flags,
                        mysql_error(mysql));
        return -1;
      }

      mysql_close(mysql);	/* Close connection to avoid error messages */
      argc=1;             /* Force SHUTDOWN to be the last command */

      if (got_pidfile)
      {
        if (opt_verbose)
        printf("Shutdown signal sent to server;  Waiting for pid file to disappear\n");

        /* Wait until pid file is gone */
        if (wait_pidfile(pidfile, last_modified, &pidfile_status))
          return -1;
        }
        break;
    }
    case ADMIN_PING:
      mysql->reconnect=0;	/* We want to know of reconnects */
      if (!mysql_ping(mysql))
      {
        if (option_silent < 2)
          puts("mysqld is alive");
      }
      else
      {
        if (mysql_errno(mysql) == CR_SERVER_GONE_ERROR)
        {
          mysql->reconnect=1;
          if (!mysql_ping(mysql))
            puts("connection was down, but mysqld is now alive");
        }
        else
	      {
          my_printf_error(0,"mysqld doesn't answer to ping, error: '%s'",
          error_flags, mysql_error(mysql));
          return -1;
        }
      }
      mysql->reconnect=1;	/* Automatic reconnect is default */
      break;

    default:
      my_printf_error(0, "Unknown command: '%-.60s'", error_flags, argv[0]);
      return 1;
    }
  }
  return 0;
}

#include <help_start.h>

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s on %s\n",my_progname,ADMIN_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

static void usage(void)
{
  print_version();
  puts("Copyright (C) 2000-2006 MySQL AB");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  puts("Administration program for the mysqld daemon.");
  printf("Usage: %s [OPTIONS] command command....\n", my_progname);
  my_print_help(my_long_options);
  puts("\
  ping         Check if server is down\n\
  shutdown     Take server down\n");
}

#include <help_end.h>

static bool get_pidfile(MYSQL *mysql, char *pidfile)
{
  MYSQL_RES* result;

  if (mysql_query(mysql, "SHOW VARIABLES LIKE 'pid_file'"))
  {
    my_printf_error(0, "query failed; error: '%s'", error_flags,
		    mysql_error(mysql));
  }
  result = mysql_store_result(mysql);
  if (result)
  {
    MYSQL_ROW row=mysql_fetch_row(result);
    if (row)
      strmov(pidfile, row[1]);
    mysql_free_result(result);
    return row == 0;				/* Error if row = 0 */
  }
  return 1;					/* Error */
}

/*
  Return 1 if pid file didn't disappear or change
*/
static bool wait_pidfile(char *pidfile, time_t last_modified,
                         struct stat *pidfile_status)
{
  char buff[FN_REFLEN];
  int error= 1;
  uint count= 0;

  system_filename(buff, pidfile);
  do
  {
    int fd;
    if ((fd= my_open(buff, O_RDONLY, MYF(0))) < 0)
    {
      error= 0;
      break;
    }
    (void) my_close(fd,MYF(0));
    if (last_modified && !stat(pidfile, pidfile_status))
    {
      if (last_modified != pidfile_status->st_mtime)
      {
        /* File changed;  Let's assume that mysqld did restart */
        if (opt_verbose)
          printf("pid file '%s' changed while waiting for it to disappear!\n\
                 mysqld did probably restart\n", buff);
        error= 0;
        break;
      }
    }
    if (count++ == opt_shutdown_timeout)
      break;
    sleep(1);
  } while (!interrupted);

  if (error)
  {
    fprintf(stderr,
	    "Warning;  Aborted waiting on pid file: '%s' after %d seconds\n",
	    buff, count-1);
  }
  return(error);
}

