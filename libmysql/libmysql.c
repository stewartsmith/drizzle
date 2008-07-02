/* Copyright (C) 2000-2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation.

   There are special exceptions to the terms and conditions of the GPL as it
   is applied to this software. View the full text of the exception in file
   EXCEPTIONS-CLIENT in the directory of this software distribution.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include <my_sys.h>
#include <my_time.h>
#include <mysys_err.h>
#include <m_string.h>
#include <m_ctype.h>
#include "mysql.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include "errmsg.h"
#include <violite.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#ifdef	 HAVE_PWD_H
#include <pwd.h>
#endif
#if !defined(MSDOS) && !defined(__WIN__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SELECT_H
#include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif /* !defined(MSDOS) && !defined(__WIN__) */
#ifdef HAVE_POLL
#include <sys/poll.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#if defined(THREAD) && !defined(__WIN__)
#include <my_pthread.h>				/* because of signal()	*/
#endif
#ifndef INADDR_NONE
#define INADDR_NONE	-1
#endif

#include <sql_common.h>
#include "client_settings.h"

#undef net_buffer_length
#undef max_allowed_packet

ulong 		net_buffer_length=8192;
ulong		max_allowed_packet= 1024L*1024L*1024L;


#ifdef EMBEDDED_LIBRARY
#undef net_flush
my_bool	net_flush(NET *net);
#endif

#if defined(MSDOS) || defined(__WIN__)
/* socket_errno is defined in my_global.h for all platforms */
#define perror(A)
#else
#include <errno.h>
#define SOCKET_ERROR -1
#endif /* __WIN__ */

/*
  If allowed through some configuration, then this needs to
  be changed
*/
#define MAX_LONG_DATA_LENGTH 8192
#define unsigned_field(A) ((A)->flags & UNSIGNED_FLAG)

static void append_wild(char *to,char *end,const char *wild);
sig_handler my_pipe_sig_handler(int sig);

static my_bool mysql_client_init= 0;
static my_bool org_my_init_done= 0;


/*
  Initialize the MySQL client library

  SYNOPSIS
    mysql_server_init()

  NOTES
    Should be called before doing any other calls to the MySQL
    client library to initialize thread specific variables etc.
    It's called by mysql_init() to ensure that things will work for
    old not threaded applications that doesn't call mysql_server_init()
    directly.

  RETURN
    0  ok
    1  could not initialize environment (out of memory or thread keys)
*/

int STDCALL mysql_server_init(int argc __attribute__((unused)),
			      char **argv __attribute__((unused)),
			      char **groups __attribute__((unused)))
{
  int result= 0;
  if (!mysql_client_init)
  {
    mysql_client_init=1;
    org_my_init_done=my_init_done;
    if (my_init())				/* Will init threads */
      return 1;
    init_client_errs();
    if (!mysql_port)
    {
      mysql_port = MYSQL_PORT;
#ifndef MSDOS
      {
	struct servent *serv_ptr;
	char	*env;

        /*
          if builder specifically requested a default port, use that
          (even if it coincides with our factory default).
          only if they didn't do we check /etc/services (and, failing
          on that, fall back to the factory default of 3306).
          either default can be overridden by the environment variable
          MYSQL_TCP_PORT, which in turn can be overridden with command
          line options.
        */

#if MYSQL_PORT_DEFAULT == 0
        if ((serv_ptr = getservbyname("mysql", "tcp")))
          mysql_port = (uint) ntohs((ushort) serv_ptr->s_port);
#endif
        if ((env = getenv("MYSQL_TCP_PORT")))
          mysql_port =(uint) atoi(env);
      }
#endif
    }
    if (!mysql_unix_port)
    {
      char *env;
      mysql_unix_port = (char*) MYSQL_UNIX_ADDR;
      if ((env = getenv("MYSQL_UNIX_PORT")))
	mysql_unix_port = env;
    }
    mysql_debug(NullS);
#if defined(SIGPIPE)
    (void) signal(SIGPIPE, SIG_IGN);
#endif
  }
#ifdef THREAD
  else
    result= (int)my_thread_init();         /* Init if new thread */
#endif
  return result;
}


/*
  Free all memory and resources used by the client library

  NOTES
    When calling this there should not be any other threads using
    the library.

    To make things simpler when used with windows dll's (which calls this
    function automaticly), it's safe to call this function multiple times.
*/


void STDCALL mysql_server_end()
{
  if (!mysql_client_init)
    return;

  finish_client_errs();
  vio_end();

  /* If library called my_init(), free memory allocated by it */
  if (!org_my_init_done)
  {
    my_end(MY_DONT_FREE_DBUG);
    /* Remove TRACING, if enabled by mysql_debug() */
    DBUG_POP();
  }
  else
  {
    free_charsets();
    mysql_thread_end();
  }

  mysql_client_init= org_my_init_done= 0;
#ifdef EMBEDDED_SERVER
  if (stderror_file)
  {
    fclose(stderror_file);
    stderror_file= 0;
  }
#endif
}

static MYSQL_PARAMETERS mysql_internal_parameters=
{&max_allowed_packet, &net_buffer_length, 0};

MYSQL_PARAMETERS *STDCALL mysql_get_parameters(void)
{
  return &mysql_internal_parameters;
}

my_bool STDCALL mysql_thread_init()
{
#ifdef THREAD
  return my_thread_init();
#else
  return 0;
#endif
}

void STDCALL mysql_thread_end()
{
#ifdef THREAD
  my_thread_end();
#endif
}


/*
  Expand wildcard to a sql string
*/

static void
append_wild(char *to, char *end, const char *wild)
{
  end-=5;					/* Some extra */
  if (wild && wild[0])
  {
    to=strmov(to," like '");
    while (*wild && to < end)
    {
      if (*wild == '\\' || *wild == '\'')
	*to++='\\';
      *to++= *wild++;
    }
    if (*wild)					/* Too small buffer */
      *to++='%';				/* Nicer this way */
    to[0]='\'';
    to[1]=0;
  }
}


/**************************************************************************
  Init debugging if MYSQL_DEBUG environment variable is found
**************************************************************************/

void STDCALL
mysql_debug(const char *debug __attribute__((unused)))
{
#ifndef DBUG_OFF
  char	*env;
  if (debug)
  {
    DBUG_PUSH(debug);
  }
  else if ((env = getenv("MYSQL_DEBUG")))
  {
    DBUG_PUSH(env);
#if !defined(_WINVER) && !defined(WINVER)
    puts("\n-------------------------------------------------------");
    puts("MYSQL_DEBUG found. libmysql started with the following:");
    puts(env);
    puts("-------------------------------------------------------\n");
#else
    {
      char buff[80];
      buff[sizeof(buff)-1]= 0;
      strxnmov(buff,sizeof(buff)-1,"libmysql: ", env, NullS);
      MessageBox((HWND) 0,"Debugging variable MYSQL_DEBUG used",buff,MB_OK);
    }
#endif
  }
#endif
}


/**************************************************************************
  Ignore SIGPIPE handler
   ARGSUSED
**************************************************************************/

sig_handler
my_pipe_sig_handler(int sig __attribute__((unused)))
{
  DBUG_PRINT("info",("Hit by signal %d",sig));
#ifdef DONT_REMEMBER_SIGNAL
  (void) signal(SIGPIPE, my_pipe_sig_handler);
#endif
}


/**************************************************************************
  Connect to sql server
  If host == 0 then use localhost
**************************************************************************/

#ifdef USE_OLD_FUNCTIONS
MYSQL * STDCALL
mysql_connect(MYSQL *mysql,const char *host,
	      const char *user, const char *passwd)
{
  MYSQL *res;
  mysql=mysql_init(mysql);			/* Make it thread safe */
  {
    DBUG_ENTER("mysql_connect");
    if (!(res=mysql_real_connect(mysql,host,user,passwd,NullS,0,NullS,0)))
    {
      if (mysql->free_me)
	my_free((uchar*) mysql,MYF(0));
    }
    mysql->reconnect= 1;
    DBUG_RETURN(res);
  }
}
#endif


/**************************************************************************
  Change user and database
**************************************************************************/

int cli_read_change_user_result(MYSQL *mysql, char *buff, const char *passwd)
{
  NET *net= &mysql->net;
  ulong pkt_length;

  pkt_length= cli_safe_read(mysql);
  
  if (pkt_length == packet_error)
    return 1;

  if (pkt_length == 1 && net->read_pos[0] == 254 &&
      mysql->server_capabilities & CLIENT_SECURE_CONNECTION)
  {
    /*
      By sending this very specific reply server asks us to send scrambled
      password in old format. The reply contains scramble_323.
    */
    scramble_323(buff, mysql->scramble, passwd);
    if (my_net_write(net, (uchar*) buff, SCRAMBLE_LENGTH_323 + 1) ||
        net_flush(net))
    {
      set_mysql_error(mysql, CR_SERVER_LOST, unknown_sqlstate);
      return 1;
    }
    /* Read what server thinks about out new auth message report */
    if (cli_safe_read(mysql) == packet_error)
      return 1;
  }
  return 0;
}

my_bool	STDCALL mysql_change_user(MYSQL *mysql, const char *user,
				  const char *passwd, const char *db)
{
  char buff[USERNAME_LENGTH+SCRAMBLED_PASSWORD_CHAR_LENGTH+NAME_LEN+2];
  char *end= buff;
  int rc;
  CHARSET_INFO *saved_cs= mysql->charset;

  DBUG_ENTER("mysql_change_user");

  /* Get the connection-default character set. */

  if (mysql_init_character_set(mysql))
  {
    mysql->charset= saved_cs;
    DBUG_RETURN(TRUE);
  }

  /* Use an empty string instead of NULL. */

  if (!user)
    user="";
  if (!passwd)
    passwd="";

  /* Store user into the buffer */
  end= strmake(end, user, USERNAME_LENGTH) + 1;

  /* write scrambled password according to server capabilities */
  if (passwd[0])
  {
    if (mysql->server_capabilities & CLIENT_SECURE_CONNECTION)
    {
      *end++= SCRAMBLE_LENGTH;
      scramble(end, mysql->scramble, passwd);
      end+= SCRAMBLE_LENGTH;
    }
    else
    {
      scramble_323(end, mysql->scramble, passwd);
      end+= SCRAMBLE_LENGTH_323 + 1;
    }
  }
  else
    *end++= '\0';                               /* empty password */
  /* Add database if needed */
  end= strmake(end, db ? db : "", NAME_LEN) + 1;

  /* Add character set number. */

  if (mysql->server_capabilities & CLIENT_SECURE_CONNECTION)
  {
    int2store(end, (ushort) mysql->charset->number);
    end+= 2;
  }

  /* Write authentication package */
  simple_command(mysql,COM_CHANGE_USER, (uchar*) buff, (ulong) (end-buff), 1);

  rc= (*mysql->methods->read_change_user_result)(mysql, buff, passwd);

  if (rc == 0)
  {
    /* Free old connect information */
    my_free(mysql->user,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->passwd,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->db,MYF(MY_ALLOW_ZERO_PTR));

    /* alloc new connect information */
    mysql->user=  my_strdup(user,MYF(MY_WME));
    mysql->passwd=my_strdup(passwd,MYF(MY_WME));
    mysql->db=    db ? my_strdup(db,MYF(MY_WME)) : 0;
  }
  else
  {
    mysql->charset= saved_cs;
  }

  DBUG_RETURN(rc);
}

#if defined(HAVE_GETPWUID) && defined(NO_GETPWUID_DECL)
struct passwd *getpwuid(uid_t);
char* getlogin(void);
#endif

void read_user_name(char *name)
{
  DBUG_ENTER("read_user_name");
  if (geteuid() == 0)
    (void) strmov(name,"root");		/* allow use of surun */
  else
  {
#ifdef HAVE_GETPWUID
    struct passwd *skr;
    const char *str;
    if ((str=getlogin()) == NULL)
    {
      if ((skr=getpwuid(geteuid())) != NULL)
	str=skr->pw_name;
      else if (!(str=getenv("USER")) && !(str=getenv("LOGNAME")) &&
	       !(str=getenv("LOGIN")))
	str="UNKNOWN_USER";
    }
    (void) strmake(name,str,USERNAME_LENGTH);
#elif HAVE_CUSERID
    (void) cuserid(name);
#else
    strmov(name,"UNKNOWN_USER");
#endif
  }
  DBUG_VOID_RETURN;
}

my_bool handle_local_infile(MYSQL *mysql, const char *net_filename)
{
  my_bool result= 1;
  uint packet_length=MY_ALIGN(mysql->net.max_packet-16,IO_SIZE);
  NET *net= &mysql->net;
  int readcount;
  void *li_ptr;          /* pass state to local_infile functions */
  char *buf;		/* buffer to be filled by local_infile_read */
  struct st_mysql_options *options= &mysql->options;
  DBUG_ENTER("handle_local_infile");

  /* check that we've got valid callback functions */
  if (!(options->local_infile_init &&
	options->local_infile_read &&
	options->local_infile_end &&
	options->local_infile_error))
  {
    /* if any of the functions is invalid, set the default */
    mysql_set_local_infile_default(mysql);
  }

  /* copy filename into local memory and allocate read buffer */
  if (!(buf=my_malloc(packet_length, MYF(0))))
  {
    set_mysql_error(mysql, CR_OUT_OF_MEMORY, unknown_sqlstate);
    DBUG_RETURN(1);
  }

  /* initialize local infile (open file, usually) */
  if ((*options->local_infile_init)(&li_ptr, net_filename,
    options->local_infile_userdata))
  {
    VOID(my_net_write(net,(const uchar*) "",0)); /* Server needs one packet */
    net_flush(net);
    strmov(net->sqlstate, unknown_sqlstate);
    net->last_errno=
      (*options->local_infile_error)(li_ptr,
                                     net->last_error,
                                     sizeof(net->last_error)-1);
    goto err;
  }

  /* read blocks of data from local infile callback */
  while ((readcount =
	  (*options->local_infile_read)(li_ptr, buf,
					packet_length)) > 0)
  {
    if (my_net_write(net, (uchar*) buf, readcount))
    {
      DBUG_PRINT("error",
		 ("Lost connection to MySQL server during LOAD DATA of local file"));
      set_mysql_error(mysql, CR_SERVER_LOST, unknown_sqlstate);
      goto err;
    }
  }

  /* Send empty packet to mark end of file */
  if (my_net_write(net, (const uchar*) "", 0) || net_flush(net))
  {
    set_mysql_error(mysql, CR_SERVER_LOST, unknown_sqlstate);
    goto err;
  }

  if (readcount < 0)
  {
    net->last_errno=
      (*options->local_infile_error)(li_ptr,
                                     net->last_error,
                                     sizeof(net->last_error)-1);
    goto err;
  }

  result=0;					/* Ok */

err:
  /* free up memory allocated with _init, usually */
  (*options->local_infile_end)(li_ptr);
  my_free(buf, MYF(0));
  DBUG_RETURN(result);
}


/****************************************************************************
  Default handlers for LOAD LOCAL INFILE
****************************************************************************/

typedef struct st_default_local_infile
{
  int fd;
  int error_num;
  const char *filename;
  char error_msg[LOCAL_INFILE_ERROR_LEN];
} default_local_infile_data;


/*
  Open file for LOAD LOCAL INFILE

  SYNOPSIS
    default_local_infile_init()
    ptr			Store pointer to internal data here
    filename		File name to open. This may be in unix format !


  NOTES
    Even if this function returns an error, the load data interface
    guarantees that default_local_infile_end() is called.

  RETURN
    0	ok
    1	error
*/

static int default_local_infile_init(void **ptr, const char *filename,
             void *userdata __attribute__ ((unused)))
{
  default_local_infile_data *data;
  char tmp_name[FN_REFLEN];

  if (!(*ptr= data= ((default_local_infile_data *)
		     my_malloc(sizeof(default_local_infile_data),  MYF(0)))))
    return 1; /* out of memory */

  data->error_msg[0]= 0;
  data->error_num=    0;
  data->filename= filename;

  fn_format(tmp_name, filename, "", "", MY_UNPACK_FILENAME);
  if ((data->fd = my_open(tmp_name, O_RDONLY, MYF(0))) < 0)
  {
    data->error_num= my_errno;
    my_snprintf(data->error_msg, sizeof(data->error_msg)-1,
                EE(EE_FILENOTFOUND), tmp_name, data->error_num);
    return 1;
  }
  return 0; /* ok */
}


/*
  Read data for LOAD LOCAL INFILE

  SYNOPSIS
    default_local_infile_read()
    ptr			Points to handle allocated by _init
    buf			Read data here
    buf_len		Ammount of data to read

  RETURN
    > 0		number of bytes read
    == 0	End of data
    < 0		Error
*/

static int default_local_infile_read(void *ptr, char *buf, uint buf_len)
{
  int count;
  default_local_infile_data*data = (default_local_infile_data *) ptr;

  if ((count= (int) my_read(data->fd, (uchar *) buf, buf_len, MYF(0))) < 0)
  {
    data->error_num= EE_READ; /* the errmsg for not entire file read */
    my_snprintf(data->error_msg, sizeof(data->error_msg)-1,
		EE(EE_READ),
		data->filename, my_errno);
  }
  return count;
}


/*
  Read data for LOAD LOCAL INFILE

  SYNOPSIS
    default_local_infile_end()
    ptr			Points to handle allocated by _init
			May be NULL if _init failed!

  RETURN
*/

static void default_local_infile_end(void *ptr)
{
  default_local_infile_data *data= (default_local_infile_data *) ptr;
  if (data)					/* If not error on open */
  {
    if (data->fd >= 0)
      my_close(data->fd, MYF(MY_WME));
    my_free(ptr, MYF(MY_WME));
  }
}


/*
  Return error from LOAD LOCAL INFILE

  SYNOPSIS
    default_local_infile_end()
    ptr			Points to handle allocated by _init
			May be NULL if _init failed!
    error_msg		Store error text here
    error_msg_len	Max lenght of error_msg

  RETURN
    error message number
*/

static int
default_local_infile_error(void *ptr, char *error_msg, uint error_msg_len)
{
  default_local_infile_data *data = (default_local_infile_data *) ptr;
  if (data)					/* If not error on open */
  {
    strmake(error_msg, data->error_msg, error_msg_len);
    return data->error_num;
  }
  /* This can only happen if we got error on malloc of handle */
  strmov(error_msg, ER(CR_OUT_OF_MEMORY));
  return CR_OUT_OF_MEMORY;
}


void
mysql_set_local_infile_handler(MYSQL *mysql,
                               int (*local_infile_init)(void **, const char *,
                               void *),
                               int (*local_infile_read)(void *, char *, uint),
                               void (*local_infile_end)(void *),
                               int (*local_infile_error)(void *, char *, uint),
                               void *userdata)
{
  mysql->options.local_infile_init=  local_infile_init;
  mysql->options.local_infile_read=  local_infile_read;
  mysql->options.local_infile_end=   local_infile_end;
  mysql->options.local_infile_error= local_infile_error;
  mysql->options.local_infile_userdata = userdata;
}


void mysql_set_local_infile_default(MYSQL *mysql)
{
  mysql->options.local_infile_init=  default_local_infile_init;
  mysql->options.local_infile_read=  default_local_infile_read;
  mysql->options.local_infile_end=   default_local_infile_end;
  mysql->options.local_infile_error= default_local_infile_error;
}


/**************************************************************************
  Do a query. If query returned rows, free old rows.
  Read data by mysql_store_result or by repeat call of mysql_fetch_row
**************************************************************************/

int STDCALL
mysql_query(MYSQL *mysql, const char *query)
{
  return mysql_real_query(mysql,query, (uint) strlen(query));
}


/**************************************************************************
  Return next field of the query results
**************************************************************************/

MYSQL_FIELD * STDCALL
mysql_fetch_field(MYSQL_RES *result)
{
  if (result->current_field >= result->field_count)
    return(NULL);
  return &result->fields[result->current_field++];
}


/**************************************************************************
  Move to a specific row and column
**************************************************************************/

void STDCALL
mysql_data_seek(MYSQL_RES *result, my_ulonglong row)
{
  MYSQL_ROWS	*tmp=0;
  DBUG_PRINT("info",("mysql_data_seek(%ld)",(long) row));
  if (result->data)
    for (tmp=result->data->data; row-- && tmp ; tmp = tmp->next) ;
  result->current_row=0;
  result->data_cursor = tmp;
}


/*************************************************************************
  put the row or field cursor one a position one got from mysql_row_tell()
  This doesn't restore any data. The next mysql_fetch_row or
  mysql_fetch_field will return the next row or field after the last used
*************************************************************************/

MYSQL_ROW_OFFSET STDCALL
mysql_row_seek(MYSQL_RES *result, MYSQL_ROW_OFFSET row)
{
  MYSQL_ROW_OFFSET return_value=result->data_cursor;
  result->current_row= 0;
  result->data_cursor= row;
  return return_value;
}


MYSQL_FIELD_OFFSET STDCALL
mysql_field_seek(MYSQL_RES *result, MYSQL_FIELD_OFFSET field_offset)
{
  MYSQL_FIELD_OFFSET return_value=result->current_field;
  result->current_field=field_offset;
  return return_value;
}


/*****************************************************************************
  List all databases
*****************************************************************************/

MYSQL_RES * STDCALL
mysql_list_dbs(MYSQL *mysql, const char *wild)
{
  char buff[255];
  DBUG_ENTER("mysql_list_dbs");

  append_wild(strmov(buff,"show databases"),buff+sizeof(buff),wild);
  if (mysql_query(mysql,buff))
    DBUG_RETURN(0);
  DBUG_RETURN (mysql_store_result(mysql));
}


/*****************************************************************************
  List all tables in a database
  If wild is given then only the tables matching wild is returned
*****************************************************************************/

MYSQL_RES * STDCALL
mysql_list_tables(MYSQL *mysql, const char *wild)
{
  char buff[255];
  DBUG_ENTER("mysql_list_tables");

  append_wild(strmov(buff,"show tables"),buff+sizeof(buff),wild);
  if (mysql_query(mysql,buff))
    DBUG_RETURN(0);
  DBUG_RETURN (mysql_store_result(mysql));
}


MYSQL_FIELD *cli_list_fields(MYSQL *mysql)
{
  MYSQL_DATA *query;
  if (!(query= cli_read_rows(mysql,(MYSQL_FIELD*) 0, 
			     protocol_41(mysql) ? 8 : 6)))
    return NULL;

  mysql->field_count= (uint) query->rows;
  return unpack_fields(query,&mysql->field_alloc,
		       mysql->field_count, 1, mysql->server_capabilities);
}


/**************************************************************************
  List all fields in a table
  If wild is given then only the fields matching wild is returned
  Instead of this use query:
  show fields in 'table' like "wild"
**************************************************************************/

MYSQL_RES * STDCALL
mysql_list_fields(MYSQL *mysql, const char *table, const char *wild)
{
  MYSQL_RES   *result;
  MYSQL_FIELD *fields;
  char	     buff[257],*end;
  DBUG_ENTER("mysql_list_fields");
  DBUG_PRINT("enter",("table: '%s'  wild: '%s'",table,wild ? wild : ""));

  end=strmake(strmake(buff, table,128)+1,wild ? wild : "",128);
  free_old_query(mysql);
  if (simple_command(mysql, COM_FIELD_LIST, (uchar*) buff,
                     (ulong) (end-buff), 1) ||
      !(fields= (*mysql->methods->list_fields)(mysql)))
    DBUG_RETURN(NULL);

  if (!(result = (MYSQL_RES *) my_malloc(sizeof(MYSQL_RES),
					 MYF(MY_WME | MY_ZEROFILL))))
    DBUG_RETURN(NULL);

  result->methods= mysql->methods;
  result->field_alloc=mysql->field_alloc;
  mysql->fields=0;
  result->field_count = mysql->field_count;
  result->fields= fields;
  result->eof=1;
  DBUG_RETURN(result);
}

/* List all running processes (threads) in server */

MYSQL_RES * STDCALL
mysql_list_processes(MYSQL *mysql)
{
  MYSQL_DATA *fields;
  uint field_count;
  uchar *pos;
  DBUG_ENTER("mysql_list_processes");

  if (simple_command(mysql,COM_PROCESS_INFO,0,0,0))
    DBUG_RETURN(0);
  free_old_query(mysql);
  pos=(uchar*) mysql->net.read_pos;
  field_count=(uint) net_field_length(&pos);
  if (!(fields = (*mysql->methods->read_rows)(mysql,(MYSQL_FIELD*) 0,
					      protocol_41(mysql) ? 7 : 5)))
    DBUG_RETURN(NULL);
  if (!(mysql->fields=unpack_fields(fields,&mysql->field_alloc,field_count,0,
				    mysql->server_capabilities)))
    DBUG_RETURN(0);
  mysql->status=MYSQL_STATUS_GET_RESULT;
  mysql->field_count=field_count;
  DBUG_RETURN(mysql_store_result(mysql));
}


#ifdef USE_OLD_FUNCTIONS
int  STDCALL
mysql_create_db(MYSQL *mysql, const char *db)
{
  DBUG_ENTER("mysql_createdb");
  DBUG_PRINT("enter",("db: %s",db));
  DBUG_RETURN(simple_command(mysql,COM_CREATE_DB,db, (ulong) strlen(db),0));
}


int  STDCALL
mysql_drop_db(MYSQL *mysql, const char *db)
{
  DBUG_ENTER("mysql_drop_db");
  DBUG_PRINT("enter",("db: %s",db));
  DBUG_RETURN(simple_command(mysql,COM_DROP_DB,db,(ulong) strlen(db),0));
}
#endif


int STDCALL
mysql_shutdown(MYSQL *mysql, enum mysql_enum_shutdown_level shutdown_level)
{
  uchar level[1];
  DBUG_ENTER("mysql_shutdown");
  level[0]= (uchar) shutdown_level;
  DBUG_RETURN(simple_command(mysql, COM_SHUTDOWN, level, 1, 0));
}


int STDCALL
mysql_refresh(MYSQL *mysql,uint options)
{
  uchar bits[1];
  DBUG_ENTER("mysql_refresh");
  bits[0]= (uchar) options;
  DBUG_RETURN(simple_command(mysql, COM_REFRESH, bits, 1, 0));
}


int STDCALL
mysql_kill(MYSQL *mysql,ulong pid)
{
  uchar buff[4];
  DBUG_ENTER("mysql_kill");
  int4store(buff,pid);
  DBUG_RETURN(simple_command(mysql,COM_PROCESS_KILL,buff,sizeof(buff),0));
}


int STDCALL
mysql_set_server_option(MYSQL *mysql, enum enum_mysql_set_option option)
{
  uchar buff[2];
  DBUG_ENTER("mysql_set_server_option");
  int2store(buff, (uint) option);
  DBUG_RETURN(simple_command(mysql, COM_SET_OPTION, buff, sizeof(buff), 0));
}


int STDCALL
mysql_dump_debug_info(MYSQL *mysql)
{
  DBUG_ENTER("mysql_dump_debug_info");
  DBUG_RETURN(simple_command(mysql,COM_DEBUG,0,0,0));
}


const char *cli_read_statistics(MYSQL *mysql)
{
  mysql->net.read_pos[mysql->packet_length]=0;	/* End of stat string */
  if (!mysql->net.read_pos[0])
  {
    set_mysql_error(mysql, CR_WRONG_HOST_INFO, unknown_sqlstate);
    return mysql->net.last_error;
  }
  return (char*) mysql->net.read_pos;
}


const char * STDCALL
mysql_stat(MYSQL *mysql)
{
  DBUG_ENTER("mysql_stat");
  if (simple_command(mysql,COM_STATISTICS,0,0,0))
    DBUG_RETURN(mysql->net.last_error);
  DBUG_RETURN((*mysql->methods->read_statistics)(mysql));
}


int STDCALL
mysql_ping(MYSQL *mysql)
{
  int res;
  DBUG_ENTER("mysql_ping");
  res= simple_command(mysql,COM_PING,0,0,0);
  if (res == CR_SERVER_LOST && mysql->reconnect)
    res= simple_command(mysql,COM_PING,0,0,0);
  DBUG_RETURN(res);
}


const char * STDCALL
mysql_get_server_info(MYSQL *mysql)
{
  return((char*) mysql->server_version);
}


const char * STDCALL
mysql_get_host_info(MYSQL *mysql)
{
  return(mysql->host_info);
}


uint STDCALL
mysql_get_proto_info(MYSQL *mysql)
{
  return (mysql->protocol_version);
}

const char * STDCALL
mysql_get_client_info(void)
{
  return (char*) MYSQL_SERVER_VERSION;
}

ulong STDCALL mysql_get_client_version(void)
{
  return MYSQL_VERSION_ID;
}

my_bool STDCALL mysql_eof(MYSQL_RES *res)
{
  return res->eof;
}

MYSQL_FIELD * STDCALL mysql_fetch_field_direct(MYSQL_RES *res,uint fieldnr)
{
  return &(res)->fields[fieldnr];
}

MYSQL_FIELD * STDCALL mysql_fetch_fields(MYSQL_RES *res)
{
  return (res)->fields;
}

MYSQL_ROW_OFFSET STDCALL mysql_row_tell(MYSQL_RES *res)
{
  return res->data_cursor;
}

MYSQL_FIELD_OFFSET STDCALL mysql_field_tell(MYSQL_RES *res)
{
  return (res)->current_field;
}

/* MYSQL */

unsigned int STDCALL mysql_field_count(MYSQL *mysql)
{
  return mysql->field_count;
}

my_ulonglong STDCALL mysql_affected_rows(MYSQL *mysql)
{
  return mysql->affected_rows;
}

my_ulonglong STDCALL mysql_insert_id(MYSQL *mysql)
{
  return mysql->insert_id;
}

const char *STDCALL mysql_sqlstate(MYSQL *mysql)
{
  return mysql ? mysql->net.sqlstate : cant_connect_sqlstate;
}

uint STDCALL mysql_warning_count(MYSQL *mysql)
{
  return mysql->warning_count;
}

const char *STDCALL mysql_info(MYSQL *mysql)
{
  return mysql->info;
}

ulong STDCALL mysql_thread_id(MYSQL *mysql)
{
  return (mysql)->thread_id;
}

const char * STDCALL mysql_character_set_name(MYSQL *mysql)
{
  return mysql->charset->csname;
}

void STDCALL mysql_get_character_set_info(MYSQL *mysql, MY_CHARSET_INFO *csinfo)
{
  csinfo->number   = mysql->charset->number;
  csinfo->state    = mysql->charset->state;
  csinfo->csname   = mysql->charset->csname;
  csinfo->name     = mysql->charset->name;
  csinfo->comment  = mysql->charset->comment;
  csinfo->mbminlen = mysql->charset->mbminlen;
  csinfo->mbmaxlen = mysql->charset->mbmaxlen;

  if (mysql->options.charset_dir)
    csinfo->dir = mysql->options.charset_dir;
  else
    csinfo->dir = charsets_dir;
}

uint STDCALL mysql_thread_safe(void)
{
#ifdef THREAD
  return 1;
#else
  return 0;
#endif
}


my_bool STDCALL mysql_embedded(void)
{
#ifdef EMBEDDED_LIBRARY
  return 1;
#else
  return 0;
#endif
}

/****************************************************************************
  Some support functions
****************************************************************************/

/*
  Functions called my my_net_init() to set some application specific variables
*/

void my_net_local_init(NET *net)
{
  net->max_packet=   (uint) net_buffer_length;
  my_net_set_read_timeout(net, CLIENT_NET_READ_TIMEOUT);
  my_net_set_write_timeout(net, CLIENT_NET_WRITE_TIMEOUT);
  net->retry_count=  1;
  net->max_packet_size= max(net_buffer_length, max_allowed_packet);
}

/*
  This function is used to create HEX string that you
  can use in a SQL statement in of the either ways:
    INSERT INTO blob_column VALUES (0xAABBCC);  (any MySQL version)
    INSERT INTO blob_column VALUES (X'AABBCC'); (4.1 and higher)
  
  The string in "from" is encoded to a HEX string.
  The result is placed in "to" and a terminating null byte is appended.
  
  The string pointed to by "from" must be "length" bytes long.
  You must allocate the "to" buffer to be at least length*2+1 bytes long.
  Each character needs two bytes, and you need room for the terminating
  null byte. When mysql_hex_string() returns, the contents of "to" will
  be a null-terminated string. The return value is the length of the
  encoded string, not including the terminating null character.

  The return value does not contain any leading 0x or a leading X' and
  trailing '. The caller must supply whichever of those is desired.
*/

ulong STDCALL
mysql_hex_string(char *to, const char *from, ulong length)
{
  char *to0= to;
  const char *end;
            
  for (end= from + length; from < end; from++)
  {
    *to++= _dig_vec_upper[((unsigned char) *from) >> 4];
    *to++= _dig_vec_upper[((unsigned char) *from) & 0x0F];
  }
  *to= '\0';
  return (ulong) (to-to0);
}

/*
  Add escape characters to a string (blob?) to make it suitable for a insert
  to should at least have place for length*2+1 chars
  Returns the length of the to string
*/

ulong STDCALL
mysql_escape_string(char *to,const char *from,ulong length)
{
  return escape_string_for_mysql(default_charset_info, to, 0, from, length);
}

ulong STDCALL
mysql_real_escape_string(MYSQL *mysql, char *to,const char *from,
			 ulong length)
{
  if (mysql->server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES)
    return escape_quotes_for_mysql(mysql->charset, to, 0, from, length);
  return escape_string_for_mysql(mysql->charset, to, 0, from, length);
}

void STDCALL
myodbc_remove_escape(MYSQL *mysql,char *name)
{
  char *to;
#ifdef USE_MB
  my_bool use_mb_flag=use_mb(mysql->charset);
  char *end;
  if (use_mb_flag)
    for (end=name; *end ; end++) ;
#endif

  for (to=name ; *name ; name++)
  {
#ifdef USE_MB
    int l;
    if (use_mb_flag && (l = my_ismbchar( mysql->charset, name , end ) ) )
    {
      while (l--)
	*to++ = *name++;
      name--;
      continue;
    }
#endif
    if (*name == '\\' && name[1])
      name++;
    *to++= *name;
  }
  *to=0;
}

/********************************************************************
 Implementation of new client API for 4.1 version.

 mysql_stmt_* are real prototypes used by applications.

 To make API work in embedded library all functions performing
 real I/O are prefixed with 'cli_' (abbreviated from 'Call Level
 Interface'). This functions are invoked via pointers set in
 MYSQL::methods structure. Embedded counterparts, prefixed with
 'emb_' reside in libmysqld/lib_sql.cc.
*********************************************************************/


static my_bool int_is_null_true= 1;		/* Used for MYSQL_TYPE_NULL */
static my_bool int_is_null_false= 0;

/********************************************************************
 Fetch and conversion of result set rows (binary protocol).
*********************************************************************/

/*
  Read date, (time, datetime) value from network buffer and store it
  in MYSQL_TIME structure.

  SYNOPSIS
    read_binary_{date,time,datetime}()
    tm    MYSQL_TIME structure to fill
    pos   pointer to current position in network buffer.
          These functions increase pos to point to the beginning of the
          next column.

  Auxiliary functions to read time (date, datetime) values from network
  buffer and store in MYSQL_TIME structure. Jointly used by conversion
  and no-conversion fetching.
*/

static void read_binary_time(MYSQL_TIME *tm, uchar **pos)
{
  /* net_field_length will set pos to the first byte of data */
  uint length= net_field_length(pos);

  if (length)
  {
    uchar *to= *pos;
    tm->neg=    to[0];

    tm->day=    (ulong) sint4korr(to+1);
    tm->hour=   (uint) to[5];
    tm->minute= (uint) to[6];
    tm->second= (uint) to[7];
    tm->second_part= (length > 8) ? (ulong) sint4korr(to+8) : 0;
    tm->year= tm->month= 0;
    if (tm->day)
    {
      /* Convert days to hours at once */
      tm->hour+= tm->day*24;
      tm->day= 0;
    }
    tm->time_type= MYSQL_TIMESTAMP_TIME;

    *pos+= length;
  }
  else
    set_zero_time(tm, MYSQL_TIMESTAMP_TIME);
}

static void read_binary_datetime(MYSQL_TIME *tm, uchar **pos)
{
  uint length= net_field_length(pos);

  if (length)
  {
    uchar *to= *pos;

    tm->neg=    0;
    tm->year=   (uint) sint2korr(to);
    tm->month=  (uint) to[2];
    tm->day=    (uint) to[3];

    if (length > 4)
    {
      tm->hour=   (uint) to[4];
      tm->minute= (uint) to[5];
      tm->second= (uint) to[6];
    }
    else
      tm->hour= tm->minute= tm->second= 0;
    tm->second_part= (length > 7) ? (ulong) sint4korr(to+7) : 0;
    tm->time_type= MYSQL_TIMESTAMP_DATETIME;

    *pos+= length;
  }
  else
    set_zero_time(tm, MYSQL_TIMESTAMP_DATETIME);
}

static void read_binary_date(MYSQL_TIME *tm, uchar **pos)
{
  uint length= net_field_length(pos);

  if (length)
  {
    uchar *to= *pos;
    tm->year =  (uint) sint2korr(to);
    tm->month=  (uint) to[2];
    tm->day= (uint) to[3];

    tm->hour= tm->minute= tm->second= 0;
    tm->second_part= 0;
    tm->neg= 0;
    tm->time_type= MYSQL_TIMESTAMP_DATE;

    *pos+= length;
  }
  else
    set_zero_time(tm, MYSQL_TIMESTAMP_DATE);
}


/*
  Check that two field types are binary compatible i. e.
  have equal representation in the binary protocol and
  require client-side buffers of the same type.

  SYNOPSIS
    is_binary_compatible()
    type1   parameter type supplied by user
    type2   field type, obtained from result set metadata

  RETURN
    TRUE or FALSE
*/

static my_bool is_binary_compatible(enum enum_field_types type1,
                                    enum enum_field_types type2)
{
  static const enum enum_field_types
    range1[]= { MYSQL_TYPE_SHORT, MYSQL_TYPE_YEAR, MYSQL_TYPE_NULL },
    range2[]= { MYSQL_TYPE_INT24, MYSQL_TYPE_LONG, MYSQL_TYPE_NULL },
    range3[]= { MYSQL_TYPE_DATETIME, MYSQL_TYPE_TIMESTAMP, MYSQL_TYPE_NULL },
    range4[]= { MYSQL_TYPE_ENUM, MYSQL_TYPE_SET, MYSQL_TYPE_TINY_BLOB,
                MYSQL_TYPE_MEDIUM_BLOB, MYSQL_TYPE_LONG_BLOB, MYSQL_TYPE_BLOB,
                MYSQL_TYPE_VAR_STRING, MYSQL_TYPE_STRING, MYSQL_TYPE_GEOMETRY,
                MYSQL_TYPE_DECIMAL, MYSQL_TYPE_NULL };
  static const enum enum_field_types
   *range_list[]= { range1, range2, range3, range4 },
   **range_list_end= range_list + sizeof(range_list)/sizeof(*range_list);
   const enum enum_field_types **range, *type;

  if (type1 == type2)
    return TRUE;
  for (range= range_list; range != range_list_end; ++range)
  {
    /* check that both type1 and type2 are in the same range */
    my_bool type1_found= FALSE, type2_found= FALSE;
    for (type= *range; *type != MYSQL_TYPE_NULL; type++)
    {
      type1_found|= type1 == *type;
      type2_found|= type2 == *type;
    }
    if (type1_found || type2_found)
      return type1_found && type2_found;
  }
  return FALSE;
}


int cli_unbuffered_fetch(MYSQL *mysql, char **row)
{
  if (packet_error == cli_safe_read(mysql))
    return 1;

  *row= ((mysql->net.read_pos[0] == 254) ? NULL :
	 (char*) (mysql->net.read_pos+1));
  return 0;
}

/********************************************************************
 Transactional APIs
*********************************************************************/

/*
  Commit the current transaction
*/

my_bool STDCALL mysql_commit(MYSQL * mysql)
{
  DBUG_ENTER("mysql_commit");
  DBUG_RETURN((my_bool) mysql_real_query(mysql, "commit", 6));
}

/*
  Rollback the current transaction
*/

my_bool STDCALL mysql_rollback(MYSQL * mysql)
{
  DBUG_ENTER("mysql_rollback");
  DBUG_RETURN((my_bool) mysql_real_query(mysql, "rollback", 8));
}


/*
  Set autocommit to either true or false
*/

my_bool STDCALL mysql_autocommit(MYSQL * mysql, my_bool auto_mode)
{
  DBUG_ENTER("mysql_autocommit");
  DBUG_PRINT("enter", ("mode : %d", auto_mode));

  DBUG_RETURN((my_bool) mysql_real_query(mysql, auto_mode ?
                                         "set autocommit=1":"set autocommit=0",
                                         16));
}


/********************************************************************
 Multi query execution + SPs APIs
*********************************************************************/

/*
  Returns true/false to indicate whether any more query results exist
  to be read using mysql_next_result()
*/

my_bool STDCALL mysql_more_results(MYSQL *mysql)
{
  my_bool res;
  DBUG_ENTER("mysql_more_results");

  res= ((mysql->server_status & SERVER_MORE_RESULTS_EXISTS) ? 1: 0);
  DBUG_PRINT("exit",("More results exists ? %d", res));
  DBUG_RETURN(res);
}


/*
  Reads and returns the next query results
*/
int STDCALL mysql_next_result(MYSQL *mysql)
{
  DBUG_ENTER("mysql_next_result");

  if (mysql->status != MYSQL_STATUS_READY)
  {
    set_mysql_error(mysql, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);
    DBUG_RETURN(1);
  }

  net_clear_error(&mysql->net);
  mysql->affected_rows= ~(my_ulonglong) 0;

  if (mysql->server_status & SERVER_MORE_RESULTS_EXISTS)
    DBUG_RETURN((*mysql->methods->next_result)(mysql));

  DBUG_RETURN(-1);				/* No more results */
}


MYSQL_RES * STDCALL mysql_use_result(MYSQL *mysql)
{
  return (*mysql->methods->use_result)(mysql);
}

my_bool STDCALL mysql_read_query_result(MYSQL *mysql)
{
  return (*mysql->methods->read_query_result)(mysql);
}

