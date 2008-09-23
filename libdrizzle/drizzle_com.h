/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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
** Common definition between mysql server & client
*/

#ifndef _libdrizzle_drizzle_com_h
#define _libdrizzle_drizzle_com_h

#include <config.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

/*
   This is included in the server and in the client.
   Options for select set by the yacc parser (stored in lex->options).

   XXX:
   log_event.h defines OPTIONS_WRITTEN_TO_BIN_LOG to specify what THD
   options list are written into binlog. These options can NOT change their
   values, or it will break replication between version.

   context is encoded as following:
   SELECT - SELECT_LEX_NODE::options
   THD    - THD::options
   intern - neither. used only as
            func(..., select_node->options | thd->options | OPTION_XXX, ...)

   TODO: separate three contexts above, move them to separate bitfields.
*/

#define SELECT_DISTINCT         (1U << 0)     // SELECT, user
#define SELECT_STRAIGHT_JOIN    (1U << 1)     // SELECT, user
#define SELECT_DESCRIBE         (1U << 2)     // SELECT, user
#define SELECT_SMALL_RESULT     (1U << 3)     // SELECT, user
#define SELECT_BIG_RESULT       (1U << 4)     // SELECT, user
#define OPTION_FOUND_ROWS       (1U << 5)     // SELECT, user
#define SELECT_NO_JOIN_CACHE    (1U << 7)     // intern
#define OPTION_BIG_TABLES       (1U << 8)     // THD, user
#define OPTION_BIG_SELECTS      (1U << 9)     // THD, user
#define OPTION_LOG_OFF          (1U << 10)    // THD, user
#define OPTION_QUOTE_SHOW_CREATE (1U << 11)   // THD, user, unused
#define TMP_TABLE_ALL_COLUMNS   (1U << 12)    // SELECT, intern
#define OPTION_WARNINGS         (1U << 13)    // THD, user
#define OPTION_AUTO_IS_NULL     (1U << 14)    // THD, user, binlog
#define OPTION_FOUND_COMMENT    (1U << 15)    // SELECT, intern, parser
#define OPTION_SAFE_UPDATES     (1U << 16)    // THD, user
#define OPTION_BUFFER_RESULT    (1U << 17)    // SELECT, user
#define OPTION_BIN_LOG          (1U << 18)    // THD, user
#define OPTION_NOT_AUTOCOMMIT   (1U << 19)    // THD, user
#define OPTION_BEGIN            (1U << 20)    // THD, intern
#define OPTION_TABLE_LOCK       (1U << 21)    // THD, intern
#define OPTION_QUICK            (1U << 22)    // SELECT (for DELETE)
#define OPTION_KEEP_LOG         (1U << 23)    // THD, user

/* The following is used to detect a conflict with DISTINCT */
#define SELECT_ALL              (1U << 24)    // SELECT, user, parser

/** The following can be set when importing tables in a 'wrong order'
   to suppress foreign key checks */
#define OPTION_NO_FOREIGN_KEY_CHECKS    (1U << 26) // THD, user, binlog
/** The following speeds up inserts to InnoDB tables by suppressing unique
   key checks in some cases */
#define OPTION_RELAXED_UNIQUE_CHECKS    (1U << 27) // THD, user, binlog
#define SELECT_NO_UNLOCK                (1U << 28) // SELECT, intern
#define OPTION_SCHEMA_TABLE             (1U << 29) // SELECT, intern
/** Flag set if setup_tables already done */
#define OPTION_SETUP_TABLES_DONE        (1U << 30) // intern
/** If not set then the thread will ignore all warnings with level notes. */
#define OPTION_SQL_NOTES                (1U << 31) // THD, user
/**
  Force the used temporary table to be a MyISAM table (because we will use
  fulltext functions when reading from it.
*/
#define TMP_TABLE_FORCE_MYISAM          (1UL << 32)
#define OPTION_PROFILING                (1UL << 33)

/*
  Dont report errors for individual rows,
  But just report error on commit (or read ofcourse)
*/
#define OPTION_ALLOW_BATCH              (1UL << 33) // THD, intern (slave)

/**
  Maximum length of time zone name that we support
  (Time zone name is char(64) in db). mysqlbinlog needs it.
*/
#define MAX_TIME_ZONE_NAME_LENGTH       (NAME_LEN + 1)

#define HOSTNAME_LENGTH 60
#define SYSTEM_CHARSET_MBMAXLEN 4
#define NAME_CHAR_LEN	64              /* Field/table name length */
#define USERNAME_CHAR_LENGTH 16
#define NAME_LEN                (NAME_CHAR_LEN*SYSTEM_CHARSET_MBMAXLEN)
#define USERNAME_LENGTH         (USERNAME_CHAR_LENGTH*SYSTEM_CHARSET_MBMAXLEN)

#define SERVER_VERSION_LENGTH 60
#define SQLSTATE_LENGTH 5

/*
  Maximum length of comments
*/
#define TABLE_COMMENT_MAXLEN 2048
#define COLUMN_COMMENT_MAXLEN 1024
#define INDEX_COMMENT_MAXLEN 1024


/*
  USER_HOST_BUFF_SIZE -- length of string buffer, that is enough to contain
  username and hostname parts of the user identifier with trailing zero in
  MySQL standard format:
  user_name_part@host_name_part\0
*/
#define USER_HOST_BUFF_SIZE HOSTNAME_LENGTH + USERNAME_LENGTH + 2

#define LOCAL_HOST	"localhost"
#define LOCAL_HOST_NAMEDPIPE "."

/*
  You should add new commands to the end of this list, otherwise old
  servers won't be able to handle them as 'unsupported'.
*/

enum enum_server_command
{
  COM_SLEEP, COM_QUIT, COM_INIT_DB, COM_QUERY, COM_FIELD_LIST,
  COM_CREATE_DB, COM_DROP_DB, COM_REFRESH, COM_SHUTDOWN,
  COM_PROCESS_INFO, COM_CONNECT, COM_PROCESS_KILL, COM_PING,
  COM_TIME, COM_CHANGE_USER, COM_BINLOG_DUMP,
  COM_CONNECT_OUT, COM_REGISTER_SLAVE,
  COM_SET_OPTION, COM_DAEMON,
  /* don't forget to update const char *command_name[] in sql_parse.cc */

  /* Must be last */
  COM_END
};


/*
  Length of random string sent by server on handshake; this is also length of
  obfuscated password, recieved from client
*/
#define SCRAMBLE_LENGTH 20
#define SCRAMBLE_LENGTH_323 8
/* length of password stored in the db: new passwords are preceeded with '*' */
#define SCRAMBLED_PASSWORD_CHAR_LENGTH (SCRAMBLE_LENGTH*2+1)
#define SCRAMBLED_PASSWORD_CHAR_LENGTH_323 (SCRAMBLE_LENGTH_323*2)


#define NOT_NULL_FLAG	1		/* Field can't be NULL */
#define PRI_KEY_FLAG	2		/* Field is part of a primary key */
#define UNIQUE_KEY_FLAG 4		/* Field is part of a unique key */
#define MULTIPLE_KEY_FLAG 8		/* Field is part of a key */
#define BLOB_FLAG	16		/* Field is a blob */
#define UNSIGNED_FLAG	32		/* Field is unsigned */
#define DECIMAL_FLAG	64		/* Field is zerofill */
#define BINARY_FLAG	128		/* Field is binary   */

/* The following are only sent to new clients */
#define ENUM_FLAG	256		/* field is an enum */
#define AUTO_INCREMENT_FLAG 512		/* field is a autoincrement field */
#define TIMESTAMP_FLAG	1024		/* Field is a timestamp */
#define SET_FLAG	2048		/* field is a set */
#define NO_DEFAULT_VALUE_FLAG 4096	/* Field doesn't have default value */
#define ON_UPDATE_NOW_FLAG 8192         /* Field is set to NOW on UPDATE */
#define NUM_FLAG	32768		/* Field is num (for clients) */
#define PART_KEY_FLAG	16384		/* Intern; Part of some key */
#define GROUP_FLAG	32768		/* Intern: Group field */
#define UNIQUE_FLAG	65536		/* Intern: Used by sql_yacc */
#define BINCMP_FLAG	131072		/* Intern: Used by sql_yacc */
#define GET_FIXED_FIELDS_FLAG (1 << 18) /* Used to get fields in item tree */
#define FIELD_IN_PART_FUNC_FLAG (1 << 19)/* Field part of partition func */
#define FIELD_IN_ADD_INDEX (1<< 20)	/* Intern: Field used in ADD INDEX */
#define FIELD_IS_RENAMED (1<< 21)       /* Intern: Field is being renamed */
#define FIELD_STORAGE_FLAGS 22          /* Storage type: bit 22, 23 and 24 */
#define COLUMN_FORMAT_FLAGS 25          /* Column format: bit 25, 26 and 27 */

#define REFRESH_GRANT		1	/* Refresh grant tables */
#define REFRESH_LOG		2	/* Start on new log file */
#define REFRESH_TABLES		4	/* close all tables */
#define REFRESH_HOSTS		8	/* Flush host cache */
#define REFRESH_STATUS		16	/* Flush status variables */
#define REFRESH_THREADS		32	/* Flush thread cache */
#define REFRESH_SLAVE           64      /* Reset master info and restart slave
					   thread */
#define REFRESH_MASTER          128     /* Remove all bin logs in the index
					   and truncate the index */

/* The following can't be set with mysql_refresh() */
#define REFRESH_READ_LOCK	16384	/* Lock tables for read */
#define REFRESH_FAST		32768	/* Intern flag */

/* RESET (remove all queries) from query cache */
#define REFRESH_QUERY_CACHE	65536
#define REFRESH_QUERY_CACHE_FREE 0x20000L /* pack query cache */
#define REFRESH_DES_KEY_FILE	0x40000L
#define REFRESH_USER_RESOURCES	0x80000L

#define CLIENT_LONG_PASSWORD	1	/* new more secure passwords */
#define CLIENT_FOUND_ROWS	2	/* Found instead of affected rows */
#define CLIENT_LONG_FLAG	4	/* Get all column flags */
#define CLIENT_CONNECT_WITH_DB	8	/* One can specify db on connect */
#define CLIENT_NO_SCHEMA	16	/* Don't allow database.table.column */
#define CLIENT_COMPRESS		32	/* Can use compression protocol */
#define CLIENT_ODBC		64	/* Odbc client */
#define CLIENT_LOCAL_FILES	128	/* Can use LOAD DATA LOCAL */
#define CLIENT_IGNORE_SPACE	256	/* Ignore spaces before '(' */
#define UNUSED_CLIENT_PROTOCOL_41	512	/* New 4.1 protocol */
#define CLIENT_INTERACTIVE	1024	/* This is an interactive client */
#define CLIENT_SSL              2048	/* Switch to SSL after handshake */
#define CLIENT_IGNORE_SIGPIPE   4096    /* IGNORE sigpipes */
#define CLIENT_TRANSACTIONS	8192	/* Client knows about transactions */
#define CLIENT_RESERVED         16384   /* Old flag for 4.1 protocol  */
#define CLIENT_SECURE_CONNECTION 32768  /* New 4.1 authentication */
#define CLIENT_MULTI_STATEMENTS (1UL << 16) /* Enable/disable multi-stmt support */
#define CLIENT_MULTI_RESULTS    (1UL << 17) /* Enable/disable multi-results */

#define CLIENT_SSL_VERIFY_SERVER_CERT (1UL << 30)
#define CLIENT_REMEMBER_OPTIONS (1UL << 31)

/* Gather all possible capabilites (flags) supported by the server */
#define CLIENT_ALL_FLAGS  (CLIENT_LONG_PASSWORD | \
                           CLIENT_FOUND_ROWS | \
                           CLIENT_LONG_FLAG | \
                           CLIENT_CONNECT_WITH_DB | \
                           CLIENT_NO_SCHEMA | \
                           CLIENT_COMPRESS | \
                           CLIENT_ODBC | \
                           CLIENT_LOCAL_FILES | \
                           CLIENT_IGNORE_SPACE | \
                           CLIENT_INTERACTIVE | \
                           CLIENT_SSL | \
                           CLIENT_IGNORE_SIGPIPE | \
                           CLIENT_TRANSACTIONS | \
                           CLIENT_RESERVED | \
                           CLIENT_SECURE_CONNECTION | \
                           CLIENT_MULTI_STATEMENTS | \
                           CLIENT_MULTI_RESULTS | \
                           CLIENT_SSL_VERIFY_SERVER_CERT | \
                           CLIENT_REMEMBER_OPTIONS)

/*
  Switch off the flags that are optional and depending on build flags
  If any of the optional flags is supported by the build it will be switched
  on before sending to the client during the connection handshake.
*/
#define CLIENT_BASIC_FLAGS (((CLIENT_ALL_FLAGS & ~CLIENT_SSL) \
                                               & ~CLIENT_COMPRESS) \
                                               & ~CLIENT_SSL_VERIFY_SERVER_CERT)

#define SERVER_STATUS_IN_TRANS     1	/* Transaction has started */
#define SERVER_STATUS_AUTOCOMMIT   2	/* Server in auto_commit mode */
#define SERVER_MORE_RESULTS_EXISTS 8    /* Multi query - next query exists */
#define SERVER_QUERY_NO_GOOD_INDEX_USED 16
#define SERVER_QUERY_NO_INDEX_USED      32
/*
  The server was able to fulfill the clients request and opened a
  read-only non-scrollable cursor for a query. This flag comes
  in reply to COM_STMT_EXECUTE and COM_STMT_FETCH commands.
*/
#define SERVER_STATUS_CURSOR_EXISTS 64
/*
  This flag is sent when a read-only cursor is exhausted, in reply to
  COM_STMT_FETCH command.
*/
#define SERVER_STATUS_LAST_ROW_SENT 128
#define SERVER_STATUS_DB_DROPPED        256 /* A database was dropped */
#define SERVER_STATUS_NO_BACKSLASH_ESCAPES 512
/*
  Tell clients that this query was logged to the slow query log.
  Not yet set in the server, but interface is defined for applications
  to use.  See WorkLog 4098.
*/
#define SERVER_QUERY_WAS_SLOW           1024

#define DRIZZLE_ERRMSG_SIZE	512
#define NET_READ_TIMEOUT	30		/* Timeout on read */
#define NET_WRITE_TIMEOUT	60		/* Timeout on write */
#define NET_WAIT_TIMEOUT	8*60*60		/* Wait for new query */

#define ONLY_KILL_QUERY         1

struct st_vio;					/* Only C */
typedef struct st_vio Vio;

#define MAX_TINYINT_WIDTH       3       /* Max width for a TINY w.o. sign */
#define MAX_SMALLINT_WIDTH      5       /* Max width for a SHORT w.o. sign */
#define MAX_MEDIUMINT_WIDTH     8       /* Max width for a INT24 w.o. sign */
#define MAX_INT_WIDTH           10      /* Max width for a LONG w.o. sign */
#define MAX_BIGINT_WIDTH        20      /* Max width for a LONGLONG */
#define MAX_CHAR_WIDTH		255	/* Max length for a CHAR colum */
#define MAX_BLOB_WIDTH		16777216	/* Default width for blob */

#define DRIZZLE_PROTOCOL_NO_MORE_DATA 0xFE




#define packet_error (~(uint32_t) 0)


/* Shutdown/kill enums and constants */ 

/* Bits for THD::killable. */
#define DRIZZLE_SHUTDOWN_KILLABLE_CONNECT    (unsigned char)(1 << 0)
#define DRIZZLE_SHUTDOWN_KILLABLE_TRANS      (unsigned char)(1 << 1)
#define DRIZZLE_SHUTDOWN_KILLABLE_LOCK_TABLE (unsigned char)(1 << 2)
#define DRIZZLE_SHUTDOWN_KILLABLE_UPDATE     (unsigned char)(1 << 3)

/* Start TINY at 1 because we removed DECIMAL from off the front of the enum */
enum enum_field_types { DRIZZLE_TYPE_TINY=1,
                        DRIZZLE_TYPE_LONG,
                        DRIZZLE_TYPE_DOUBLE,
                        DRIZZLE_TYPE_NULL,   DRIZZLE_TYPE_TIMESTAMP,
                        DRIZZLE_TYPE_LONGLONG,
                        DRIZZLE_TYPE_DATE,   DRIZZLE_TYPE_TIME,
                        DRIZZLE_TYPE_DATETIME,
                        DRIZZLE_TYPE_NEWDATE, DRIZZLE_TYPE_VARCHAR,
                        DRIZZLE_TYPE_NEWDECIMAL=253,
                        DRIZZLE_TYPE_ENUM=254,
                        DRIZZLE_TYPE_BLOB=255
};

enum drizzle_enum_shutdown_level {
  /*
    We want levels to be in growing order of hardness (because we use number
    comparisons). Note that DEFAULT does not respect the growing property, but
    it's ok.
  */
  SHUTDOWN_DEFAULT = 0,
  /* wait for existing connections to finish */
  SHUTDOWN_WAIT_CONNECTIONS= DRIZZLE_SHUTDOWN_KILLABLE_CONNECT,
  /* wait for existing trans to finish */
  SHUTDOWN_WAIT_TRANSACTIONS= DRIZZLE_SHUTDOWN_KILLABLE_TRANS,
  /* wait for existing updates to finish (=> no partial MyISAM update) */
  SHUTDOWN_WAIT_UPDATES= DRIZZLE_SHUTDOWN_KILLABLE_UPDATE,
  /* flush InnoDB buffers and other storage engines' buffers*/
  SHUTDOWN_WAIT_ALL_BUFFERS= (DRIZZLE_SHUTDOWN_KILLABLE_UPDATE << 1),
  /* don't flush InnoDB buffers, flush other storage engines' buffers*/
  SHUTDOWN_WAIT_CRITICAL_BUFFERS= (DRIZZLE_SHUTDOWN_KILLABLE_UPDATE << 1) + 1,
  /* Now the 2 levels of the KILL command */
  KILL_QUERY= 254,
  KILL_CONNECTION= 255
};


enum enum_cursor_type
{
  CURSOR_TYPE_NO_CURSOR= 0,
  CURSOR_TYPE_READ_ONLY= 1,
  CURSOR_TYPE_FOR_UPDATE= 2,
  CURSOR_TYPE_SCROLLABLE= 4
};


/* options for mysql_set_option */
enum enum_drizzle_set_option
{
  DRIZZLE_OPTION_MULTI_STATEMENTS_ON,
  DRIZZLE_OPTION_MULTI_STATEMENTS_OFF
};

#define net_new_transaction(net) ((net)->pkt_nr=0)

#ifdef __cplusplus
extern "C" {
#endif


  struct rand_struct {
    unsigned long seed1,seed2,max_value;
    double max_value_dbl;
  };

#ifdef __cplusplus
}
#endif

  /* The following is for user defined functions */

enum Item_result {STRING_RESULT=0, REAL_RESULT, INT_RESULT, ROW_RESULT,
                  DECIMAL_RESULT};

typedef struct st_udf_args
{
  unsigned int arg_count;		/* Number of arguments */
  enum Item_result *arg_type;		/* Pointer to item_results */
  char **args;				/* Pointer to argument */
  unsigned long *lengths;		/* Length of string arguments */
  char *maybe_null;			/* Set to 1 for all maybe_null args */
  char **attributes;                    /* Pointer to attribute name */
  unsigned long *attribute_lengths;     /* Length of attribute arguments */
  void *extension;
} UDF_ARGS;

  /* This holds information about the result */

typedef struct st_udf_init
{
  bool maybe_null;          /* 1 if function can return NULL */
  unsigned int decimals;       /* for real functions */
  unsigned long max_length;    /* For string functions */
  char *ptr;                   /* free pointer for function data */
  bool const_item;          /* 1 if function always returns the same value */
  void *extension;
} UDF_INIT;
/* 
  TODO: add a notion for determinism of the UDF. 
  See Item_udf_func::update_used_tables ()
*/

  /* Constants when using compression */
#define NET_HEADER_SIZE 4		/* standard header size */
#define COMP_HEADER_SIZE 3		/* compression header extra size */

  /* Prototypes to password functions */

#ifdef __cplusplus
extern "C" {
#endif

/*
  These functions are used for authentication by client and server and
  implemented in sql/password.c
*/

  void randominit(struct rand_struct *, uint32_t seed1, uint32_t seed2);
  double my_rnd(struct rand_struct *);
  void create_random_string(char *to, unsigned int length,
                            struct rand_struct *rand_st);

  void hash_password(uint32_t *to, const char *password, uint32_t password_len);

  void make_scrambled_password(char *to, const char *password);
  void scramble(char *to, const char *message, const char *password);
  bool check_scramble(const char *reply, const char *message,
                      const unsigned char *hash_stage2);
  void get_salt_from_password(unsigned char *res, const char *password);
  void make_password_from_salt(char *to, const unsigned char *hash_stage2);
  char *octet2hex(char *to, const char *str, unsigned int len);

/* end of password.c */

  char *get_tty_password(const char *opt_message);
  const char *drizzle_errno_to_sqlstate(unsigned int drizzle_errno);

  uint32_t net_field_length(unsigned char **packet);
  uint64_t net_field_length_ll(unsigned char **packet);
  unsigned char *net_store_length(unsigned char *pkg, uint64_t length);

/*
  Define-funktions for reading and storing in machine independent format
  (low byte first)
*/

/* Optimized store functions for Intel x86 */
#if defined(__i386__)
#define sint2korr(A)	(*((int16_t *) (A)))
#define sint3korr(A)	((int32_t) ((((unsigned char) (A)[2]) & 128) ? \
				  (((uint32_t) 255L << 24) | \
				   (((uint32_t) (unsigned char) (A)[2]) << 16) |\
				   (((uint32_t) (unsigned char) (A)[1]) << 8) | \
				   ((uint32_t) (unsigned char) (A)[0])) : \
				  (((uint32_t) (unsigned char) (A)[2]) << 16) |\
				  (((uint32_t) (unsigned char) (A)[1]) << 8) | \
				  ((uint32_t) (unsigned char) (A)[0])))
#define sint4korr(A)	(*((long *) (A)))
#define uint2korr(A)	(*((uint16_t *) (A)))
#if defined(HAVE_purify)
#define uint3korr(A)	(uint32_t) (((uint32_t) ((unsigned char) (A)[0])) +\
				  (((uint32_t) ((unsigned char) (A)[1])) << 8) +\
				  (((uint32_t) ((unsigned char) (A)[2])) << 16))
#else
/*
   ATTENTION !
   
    Please, note, uint3korr reads 4 bytes (not 3) !
    It means, that you have to provide enough allocated space !
*/
#define uint3korr(A)	(long) (*((unsigned int *) (A)) & 0xFFFFFF)
#endif /* HAVE_purify */
#define uint4korr(A)	(*((uint32_t *) (A)))
#define uint5korr(A)	((uint64_t)(((uint32_t) ((unsigned char) (A)[0])) +\
				    (((uint32_t) ((unsigned char) (A)[1])) << 8) +\
				    (((uint32_t) ((unsigned char) (A)[2])) << 16) +\
				    (((uint32_t) ((unsigned char) (A)[3])) << 24)) +\
				    (((uint64_t) ((unsigned char) (A)[4])) << 32))
#define uint6korr(A)	((uint64_t)(((uint32_t)    ((unsigned char) (A)[0]))          + \
                                     (((uint32_t)    ((unsigned char) (A)[1])) << 8)   + \
                                     (((uint32_t)    ((unsigned char) (A)[2])) << 16)  + \
                                     (((uint32_t)    ((unsigned char) (A)[3])) << 24)) + \
                         (((uint64_t) ((unsigned char) (A)[4])) << 32) +       \
                         (((uint64_t) ((unsigned char) (A)[5])) << 40))
#define uint8korr(A)	(*((uint64_t *) (A)))
#define sint8korr(A)	(*((int64_t *) (A)))
#define int2store(T,A)	*((uint16_t*) (T))= (uint16_t) (A)
#define int3store(T,A)  do { *(T)=  (unsigned char) ((A));\
                            *(T+1)=(unsigned char) (((uint32_t) (A) >> 8));\
                            *(T+2)=(unsigned char) (((A) >> 16)); } while (0)
#define int4store(T,A)	*((long *) (T))= (long) (A)
#define int5store(T,A)  do { *(T)= (unsigned char)((A));\
                             *((T)+1)=(unsigned char) (((A) >> 8));\
                             *((T)+2)=(unsigned char) (((A) >> 16));\
                             *((T)+3)=(unsigned char) (((A) >> 24)); \
                             *((T)+4)=(unsigned char) (((A) >> 32)); } while(0)
#define int6store(T,A)  do { *(T)=    (unsigned char)((A));          \
                             *((T)+1)=(unsigned char) (((A) >> 8));  \
                             *((T)+2)=(unsigned char) (((A) >> 16)); \
                             *((T)+3)=(unsigned char) (((A) >> 24)); \
                             *((T)+4)=(unsigned char) (((A) >> 32)); \
                             *((T)+5)=(unsigned char) (((A) >> 40)); } while(0)
#define int8store(T,A)	*((uint64_t *) (T))= (uint64_t) (A)

typedef union {
  double v;
  long m[2];
} doubleget_union;
#define doubleget(V,M)	\
do { doubleget_union _tmp; \
     _tmp.m[0] = *((long*)(M)); \
     _tmp.m[1] = *(((long*) (M))+1); \
     (V) = _tmp.v; } while(0)
#define doublestore(T,V) do { *((long *) T) = ((doubleget_union *)&V)->m[0]; \
			     *(((long *) T)+1) = ((doubleget_union *)&V)->m[1]; \
                         } while (0)
#define float4get(V,M)   do { *((float *) &(V)) = *((float*) (M)); } while(0)
#define float8get(V,M)   doubleget((V),(M))
#define float4store(V,M) memcpy(V, (&M), sizeof(float))
#define floatstore(T,V)  memcpy((T), (&V), sizeof(float))
#define floatget(V,M)    memcpy(&V, (M), sizeof(float))
#define float8store(V,M) doublestore((V),(M))
#else

/*
  We're here if it's not a IA-32 architecture (Win32 and UNIX IA-32 defines
  were done before)
*/
#define sint2korr(A)	(int16_t) (((int16_t) ((unsigned char) (A)[0])) +\
				 ((int16_t) ((int16_t) (A)[1]) << 8))
#define sint3korr(A)	((int32_t) ((((unsigned char) (A)[2]) & 128) ? \
				  (((uint32_t) 255L << 24) | \
				   (((uint32_t) (unsigned char) (A)[2]) << 16) |\
				   (((uint32_t) (unsigned char) (A)[1]) << 8) | \
				   ((uint32_t) (unsigned char) (A)[0])) : \
				  (((uint32_t) (unsigned char) (A)[2]) << 16) |\
				  (((uint32_t) (unsigned char) (A)[1]) << 8) | \
				  ((uint32_t) (unsigned char) (A)[0])))
#define sint4korr(A)	(int32_t) (((int32_t) ((unsigned char) (A)[0])) +\
				(((int32_t) ((unsigned char) (A)[1]) << 8)) +\
				(((int32_t) ((unsigned char) (A)[2]) << 16)) +\
				(((int32_t) ((int16_t) (A)[3]) << 24)))
#define sint8korr(A)	(int64_t) uint8korr(A)
#define uint2korr(A)	(uint16_t) (((uint16_t) ((unsigned char) (A)[0])) +\
				  ((uint16_t) ((unsigned char) (A)[1]) << 8))
#define uint3korr(A)	(uint32_t) (((uint32_t) ((unsigned char) (A)[0])) +\
				  (((uint32_t) ((unsigned char) (A)[1])) << 8) +\
				  (((uint32_t) ((unsigned char) (A)[2])) << 16))
#define uint4korr(A)	(uint32_t) (((uint32_t) ((unsigned char) (A)[0])) +\
				  (((uint32_t) ((unsigned char) (A)[1])) << 8) +\
				  (((uint32_t) ((unsigned char) (A)[2])) << 16) +\
				  (((uint32_t) ((unsigned char) (A)[3])) << 24))
#define uint5korr(A)	((uint64_t)(((uint32_t) ((unsigned char) (A)[0])) +\
				    (((uint32_t) ((unsigned char) (A)[1])) << 8) +\
				    (((uint32_t) ((unsigned char) (A)[2])) << 16) +\
				    (((uint32_t) ((unsigned char) (A)[3])) << 24)) +\
				    (((uint64_t) ((unsigned char) (A)[4])) << 32))
#define uint6korr(A)	((uint64_t)(((uint32_t)    ((unsigned char) (A)[0]))          + \
                                     (((uint32_t)    ((unsigned char) (A)[1])) << 8)   + \
                                     (((uint32_t)    ((unsigned char) (A)[2])) << 16)  + \
                                     (((uint32_t)    ((unsigned char) (A)[3])) << 24)) + \
                         (((uint64_t) ((unsigned char) (A)[4])) << 32) +       \
                         (((uint64_t) ((unsigned char) (A)[5])) << 40))
#define uint8korr(A)	((uint64_t)(((uint32_t) ((unsigned char) (A)[0])) +\
				    (((uint32_t) ((unsigned char) (A)[1])) << 8) +\
				    (((uint32_t) ((unsigned char) (A)[2])) << 16) +\
				    (((uint32_t) ((unsigned char) (A)[3])) << 24)) +\
			(((uint64_t) (((uint32_t) ((unsigned char) (A)[4])) +\
				    (((uint32_t) ((unsigned char) (A)[5])) << 8) +\
				    (((uint32_t) ((unsigned char) (A)[6])) << 16) +\
				    (((uint32_t) ((unsigned char) (A)[7])) << 24))) <<\
				    32))
#define int2store(T,A)       do { uint32_t def_temp= (uint32_t) (A) ;\
                                  *((unsigned char*) (T))=  (unsigned char)(def_temp); \
                                   *((unsigned char*) (T)+1)=(unsigned char)((def_temp >> 8)); \
                             } while(0)
#define int3store(T,A)       do { /*lint -save -e734 */\
                                  *((unsigned char*)(T))=(unsigned char) ((A));\
                                  *((unsigned char*) (T)+1)=(unsigned char) (((A) >> 8));\
                                  *((unsigned char*)(T)+2)=(unsigned char) (((A) >> 16)); \
                                  /*lint -restore */} while(0)
#define int4store(T,A)       do { *((char *)(T))=(char) ((A));\
                                  *(((char *)(T))+1)=(char) (((A) >> 8));\
                                  *(((char *)(T))+2)=(char) (((A) >> 16));\
                                  *(((char *)(T))+3)=(char) (((A) >> 24)); } while(0)
#define int5store(T,A)       do { *((char *)(T))=     (char)((A));  \
                                  *(((char *)(T))+1)= (char)(((A) >> 8)); \
                                  *(((char *)(T))+2)= (char)(((A) >> 16)); \
                                  *(((char *)(T))+3)= (char)(((A) >> 24)); \
                                  *(((char *)(T))+4)= (char)(((A) >> 32)); \
		                } while(0)
#define int6store(T,A)       do { *((char *)(T))=     (char)((A)); \
                                  *(((char *)(T))+1)= (char)(((A) >> 8)); \
                                  *(((char *)(T))+2)= (char)(((A) >> 16)); \
                                  *(((char *)(T))+3)= (char)(((A) >> 24)); \
                                  *(((char *)(T))+4)= (char)(((A) >> 32)); \
                                  *(((char *)(T))+5)= (char)(((A) >> 40)); \
                                } while(0)
#define int8store(T,A)       do { uint32_t def_temp= (uint32_t) (A), def_temp2= (uint32_t) ((A) >> 32); \
                                  int4store((T),def_temp); \
                                  int4store((T+4),def_temp2); } while(0)
#ifdef WORDS_BIGENDIAN
#define float4store(T,A) do { *(T)= ((unsigned char *) &A)[3];\
                              *((T)+1)=(char) ((unsigned char *) &A)[2];\
                              *((T)+2)=(char) ((unsigned char *) &A)[1];\
                              *((T)+3)=(char) ((unsigned char *) &A)[0]; } while(0)

#define float4get(V,M)   do { float def_temp;\
                              ((unsigned char*) &def_temp)[0]=(M)[3];\
                              ((unsigned char*) &def_temp)[1]=(M)[2];\
                              ((unsigned char*) &def_temp)[2]=(M)[1];\
                              ((unsigned char*) &def_temp)[3]=(M)[0];\
                              (V)=def_temp; } while(0)
#define float8store(T,V) do { *(T)= ((unsigned char *) &V)[7];\
                              *((T)+1)=(char) ((unsigned char *) &V)[6];\
                              *((T)+2)=(char) ((unsigned char *) &V)[5];\
                              *((T)+3)=(char) ((unsigned char *) &V)[4];\
                              *((T)+4)=(char) ((unsigned char *) &V)[3];\
                              *((T)+5)=(char) ((unsigned char *) &V)[2];\
                              *((T)+6)=(char) ((unsigned char *) &V)[1];\
                              *((T)+7)=(char) ((unsigned char *) &V)[0]; } while(0)

#define float8get(V,M)   do { double def_temp;\
                              ((unsigned char*) &def_temp)[0]=(M)[7];\
                              ((unsigned char*) &def_temp)[1]=(M)[6];\
                              ((unsigned char*) &def_temp)[2]=(M)[5];\
                              ((unsigned char*) &def_temp)[3]=(M)[4];\
                              ((unsigned char*) &def_temp)[4]=(M)[3];\
                              ((unsigned char*) &def_temp)[5]=(M)[2];\
                              ((unsigned char*) &def_temp)[6]=(M)[1];\
                              ((unsigned char*) &def_temp)[7]=(M)[0];\
                              (V) = def_temp; } while(0)
#else
#define float4get(V,M)   memcpy(&V, (M), sizeof(float))
#define float4store(V,M) memcpy(V, (&M), sizeof(float))

#if defined(__FLOAT_WORD_ORDER) && (__FLOAT_WORD_ORDER == __BIG_ENDIAN)
#define doublestore(T,V) do { *(((char*)T)+0)=(char) ((unsigned char *) &V)[4];\
                              *(((char*)T)+1)=(char) ((unsigned char *) &V)[5];\
                              *(((char*)T)+2)=(char) ((unsigned char *) &V)[6];\
                              *(((char*)T)+3)=(char) ((unsigned char *) &V)[7];\
                              *(((char*)T)+4)=(char) ((unsigned char *) &V)[0];\
                              *(((char*)T)+5)=(char) ((unsigned char *) &V)[1];\
                              *(((char*)T)+6)=(char) ((unsigned char *) &V)[2];\
                              *(((char*)T)+7)=(char) ((unsigned char *) &V)[3]; }\
                         while(0)
#define doubleget(V,M)   do { double def_temp;\
                              ((unsigned char*) &def_temp)[0]=(M)[4];\
                              ((unsigned char*) &def_temp)[1]=(M)[5];\
                              ((unsigned char*) &def_temp)[2]=(M)[6];\
                              ((unsigned char*) &def_temp)[3]=(M)[7];\
                              ((unsigned char*) &def_temp)[4]=(M)[0];\
                              ((unsigned char*) &def_temp)[5]=(M)[1];\
                              ((unsigned char*) &def_temp)[6]=(M)[2];\
                              ((unsigned char*) &def_temp)[7]=(M)[3];\
                              (V) = def_temp; } while(0)
#endif /* __FLOAT_WORD_ORDER */

#define float8get(V,M)   doubleget((V),(M))
#define float8store(V,M) doublestore((V),(M))
#endif /* WORDS_BIGENDIAN */

#endif /* __i386__ */

/*
  Macro for reading 32-bit integer from network byte order (big-endian)
  from unaligned memory location.
*/
#define int4net(A)        (int32_t) (((uint32_t) ((unsigned char) (A)[3]))        |\
				  (((uint32_t) ((unsigned char) (A)[2])) << 8)  |\
				  (((uint32_t) ((unsigned char) (A)[1])) << 16) |\
				  (((uint32_t) ((unsigned char) (A)[0])) << 24))
/*
  Define-funktions for reading and storing in machine format from/to
  short/long to/from some place in memory V should be a (not
  register) variable, M is a pointer to byte
*/

#ifdef WORDS_BIGENDIAN

#define ushortget(V,M)  do { V = (uint16_t) (((uint16_t) ((unsigned char) (M)[1]))+\
                                 ((uint16_t) ((uint16_t) (M)[0]) << 8)); } while(0)
#define shortget(V,M)   do { V = (short) (((short) ((unsigned char) (M)[1]))+\
                                 ((short) ((short) (M)[0]) << 8)); } while(0)
#define longget(V,M)    do { int32_t def_temp;\
                             ((unsigned char*) &def_temp)[0]=(M)[0];\
                             ((unsigned char*) &def_temp)[1]=(M)[1];\
                             ((unsigned char*) &def_temp)[2]=(M)[2];\
                             ((unsigned char*) &def_temp)[3]=(M)[3];\
                             (V)=def_temp; } while(0)
#define ulongget(V,M)   do { uint32_t def_temp;\
                            ((unsigned char*) &def_temp)[0]=(M)[0];\
                            ((unsigned char*) &def_temp)[1]=(M)[1];\
                            ((unsigned char*) &def_temp)[2]=(M)[2];\
                            ((unsigned char*) &def_temp)[3]=(M)[3];\
                            (V)=def_temp; } while(0)
#define shortstore(T,A) do { uint32_t def_temp=(uint32_t) (A) ;\
                             *(((char*)T)+1)=(char)(def_temp); \
                             *(((char*)T)+0)=(char)(def_temp >> 8); } while(0)
#define longstore(T,A)  do { *(((char*)T)+3)=((A));\
                             *(((char*)T)+2)=(((A) >> 8));\
                             *(((char*)T)+1)=(((A) >> 16));\
                             *(((char*)T)+0)=(((A) >> 24)); } while(0)

#define floatget(V,M)     memcpy(&V, (M), sizeof(float))
#define floatstore(T, V)   memcpy((T), (&V), sizeof(float))
#define doubleget(V, M)	  memcpy(&V, (M), sizeof(double))
#define doublestore(T, V)  memcpy((T), &V, sizeof(double))
#define int64_tget(V, M)   memcpy(&V, (M), sizeof(uint64_t))
#define int64_tstore(T, V) memcpy((T), &V, sizeof(uint64_t))

#else

#define ushortget(V,M)	do { V = uint2korr(M); } while(0)
#define shortget(V,M)	do { V = sint2korr(M); } while(0)
#define longget(V,M)	do { V = sint4korr(M); } while(0)
#define ulongget(V,M)   do { V = uint4korr(M); } while(0)
#define shortstore(T,V) int2store(T,V)
#define longstore(T,V)	int4store(T,V)
#ifndef floatstore
#define floatstore(T,V)   memcpy((T), (&V), sizeof(float))
#define floatget(V,M)     memcpy(&V, (M), sizeof(float))
#endif
#ifndef doubleget
#define doubleget(V, M)	  memcpy(&V, (M), sizeof(double))
#define doublestore(T,V)  memcpy((T), &V, sizeof(double))
#endif /* doubleget */
#define int64_tget(V,M)   memcpy(&V, (M), sizeof(uint64_t))
#define int64_tstore(T,V) memcpy((T), &V, sizeof(uint64_t))

#endif /* WORDS_BIGENDIAN */


#ifdef __cplusplus
}
#endif

#define NULL_LENGTH UINT32_MAX /* For net_store_length */
#define DRIZZLE_STMT_HEADER       4
#define DRIZZLE_LONG_DATA_HEADER  6

#endif
