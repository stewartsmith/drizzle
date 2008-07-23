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

char *host= NULL, *user= NULL, *opt_password= NULL;
static bool interrupted= false, opt_verbose= false,tty_password= false; 
static uint8_t opt_protocol= MYSQL_PROTOCOL_TCP;  
static uint32_t tcp_port= 0, option_wait= 0, option_silent= 0;
static uint32_t my_end_arg;
static uint32_t opt_connect_timeout, opt_shutdown_timeout;
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
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   (char**) &tcp_port,
   (char**) &tcp_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Silently exit if one can't connect to server.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.", (char**) &user,
   (char**) &user, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
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
  case 'p':
    if (argument)
    {
      char *start=argument;
      my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
      opt_password= my_strdup(argument,MYF(MY_FAE));
      while (*argument) *argument++= 'x';   /* Destroy argument */
      if (*start)
        start[1]=0; /* Cut length of argument */
      tty_password= 0;
    }
    else 
      tty_password= 1; 
    break;
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
  drizzle_create(&mysql);
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
  if (tty_password)
    opt_password = get_tty_password(NullS);

  VOID(signal(SIGINT,endprog));			/* Here if abort */
  VOID(signal(SIGTERM,endprog));		/* Here if abort */

  if (opt_connect_timeout)
  {
    uint tmp=opt_connect_timeout;
    mysql_options(&mysql,MYSQL_OPT_CONNECT_TIMEOUT, (char*) &tmp);
  }
  /* force mysqladmin to use TCP */
  mysql_options(&mysql, MYSQL_OPT_PROTOCOL, (char*)&opt_protocol);

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
  my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
  my_free(user,MYF(MY_ALLOW_ZERO_PTR));
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
    if (mysql_real_connect(mysql,host,user,opt_password,NullS,tcp_port,NULL,0))
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

        if (mysql_errno(mysql) == CR_CONN_HOST_ERROR ||
          mysql_errno(mysql) == CR_UNKNOWN_HOST)
        {
          fprintf(stderr,"Check that drizzled is running on %s",host);
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
        fputs("Waiting for Drizzle server to answer",stderr);
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
      if (opt_verbose)
        printf("shutting down drizzled...\n");

      if (mysql_shutdown(mysql, SHUTDOWN_DEFAULT))
      {
        my_printf_error(0, "shutdown failed; error: '%s'", error_flags,
                        mysql_error(mysql));
        return -1;
      }
      mysql_close(mysql);	/* Close connection to avoid error messages */

      if (opt_verbose)
        printf("done\n");

      argc=1;             /* Force SHUTDOWN to be the last command */
      break;
    }
    case ADMIN_PING:
      mysql->reconnect=0;	/* We want to know of reconnects */
      if (!mysql_ping(mysql))
      {
        if (option_silent < 2)
          puts("drizzled is alive");
      }
      else
      {
        if (mysql_errno(mysql) == CR_SERVER_GONE_ERROR)
        {
          mysql->reconnect=1;
          if (!mysql_ping(mysql))
            puts("connection was down, but drizzled is now alive");
        }
        else
	      {
          my_printf_error(0,"drizzled doesn't answer to ping, error: '%s'",
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
  puts("Administration program for the drizzled daemon.");
  printf("Usage: %s [OPTIONS] command command....\n", my_progname);
  my_print_help(my_long_options);
  puts("\
  ping         Check if server is down\n\
  shutdown     Take server down\n");
}

#include <help_end.h>

